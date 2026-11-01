#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a fresh, private PW3 candidate offline. Never connects to a device."""
from pathlib import Path, PurePosixPath
import sys
sys.dont_write_bytecode = True
import argparse
import json
import os
import re
import secrets
import shutil
import subprocess
import tarfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from pw3_common import ALPINE, ALPINE_SHA, DEVICE, DEVICE_PROFILE, KERNEL, READER, ROOT, TOOLS, CommandError, add_work_dir, image_size_mib, workspace, clean_image, digest, require, run, save, write_atomic


def safe_path(path):
    # debugfs command files and old kernel make cannot quote arbitrary paths.
    require(re.fullmatch(r'[/A-Za-z0-9_.+-]+', str(path)) is not None,
            'Build paths must contain only letters, numbers, / . _ + -')
    return path


def tree_digest(root):
    """Stable SHA-256 over every consumed file (path, mode, content)."""
    import hashlib
    skip = {'settings', 'history', 'cache', 'clipboard', 'ota',
            'settings.reader.lua', 'history.lua'}
    digest_value = hashlib.sha256()
    for path in sorted(root.rglob('*')):
        rel_parts = path.relative_to(root).parts
        if not path.is_file() or path.is_symlink():
            continue
        if any(part in skip for part in rel_parts):
            continue
        digest_value.update(path.relative_to(root).as_posix().encode())
        digest_value.update(b'\0')
        digest_value.update(('%o' % (path.stat().st_mode & 0o777)).encode())
        digest_value.update(b'\0')
        digest_value.update(path.read_bytes())
        digest_value.update(b'\0')
    return digest_value.hexdigest()


def unpack(archive, tree):
    links = []
    seen = set()
    with tarfile.open(archive) as tar:
        for member in tar:
            rel = PurePosixPath(member.name)
            require(not rel.is_absolute() and '..' not in rel.parts, 'Unsafe rootfs archive path')
            require(str(rel) not in seen, 'Duplicate rootfs archive path')
            seen.add(str(rel))
            path = tree.joinpath(*rel.parts)
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                path.parent.mkdir(parents=True, exist_ok=True)
                with path.open('xb') as stream:
                    shutil.copyfileobj(tar.extractfile(member), stream)
            else:
                require(member.issym(), 'Unsupported rootfs archive member')
                links.append((path, member.linkname))
                continue
            path.chmod(member.mode & 0o777)
        # No subsequent archive extraction follows symlinks.  Reject link
        # targets that are absolute or escape the destination tree.
        for path, target in links:
            require(not path.exists(), 'Link collides with rootfs directory')
            target_path = PurePosixPath(target)
            if target_path.is_absolute():
                # Absolute links are normal inside a rootfs (for example
                # /bin/busybox).  They resolve inside the image at runtime,
                # never on the build host.  "../" segments must still stay
                # inside the archive root.
                resolved = (tree / target_path.as_posix().lstrip('/')).resolve(strict=False)
            else:
                resolved = (path.parent / target_path).resolve(strict=False)
            require(resolved.is_relative_to(tree.resolve()),
                    'Symlink escapes rootfs tree: '+str(target))
            path.symlink_to(target)


def build(args):
    os.umask(0o077)
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH', '0'))
    os.environ['SOURCE_DATE_EPOCH'] = str(epoch)
    # The vendor kernel embeds these values in UTS_VERSION/module metadata.
    os.environ.setdefault('KBUILD_BUILD_TIMESTAMP', 'Thu, 01 Jan 1970 00:00:00 +0000')
    os.environ.setdefault('KBUILD_BUILD_USER', 'gleworks')
    os.environ.setdefault('KBUILD_BUILD_HOST', 'gleworks')
    os.environ.setdefault('KBUILD_BUILD_VERSION', '1')
    out = safe_path((args.output or workspace(args.work_dir)/'build'/('pw3-'+args.variant+'-'+secrets.token_hex(6))).absolute())
    require(not out.exists(), 'Output must be new; existing builds are retained')
    for path in [args.rootfs, args.modules, args.p1_backup, args.return_launcher]:
        digest(path)
    require(digest(args.rootfs) == ALPINE_SHA, 'Pinned Alpine 3.24.3 armv7 archive mismatch')
    require(args.p1_backup.stat().st_size == 471859200, 'Expected exact PW3 p1 backup')
    require(args.variant == 'reader' or args.reader is None, '--reader requires --variant reader')
    if args.variant == 'reader':
        require(args.reader is not None, '--reader is required for the reader variant')
        reader_source = json.loads((READER/'source.json').read_text())
        require((args.reader/'git-rev').read_text().strip() == reader_source['release'], 'Expected pinned KOReader kindlepw2 release bundle')
        require(tree_digest(args.reader) == reader_source['bundle_tree_sha256'],
                'KOReader bundle SHA-256 differs from the pinned release')
    for path in [args.kernel_source, args.kernel_build, args.musl_prefix, args.linux_headers]:
        safe_path(path.resolve())
        require(path.is_dir(), 'Missing build directory: '+str(path))
    for tool in ['debugfs', 'mkfs.ext3', 'e2fsck', 'tune2fs', 'patch', args.make, args.hostcc]:
        require(shutil.which(tool), 'Missing host tool: '+tool)
    require(shutil.which(args.qemu_arm), 'Missing qemu-arm')
    require(shutil.which(args.cross_prefix+'gcc'), 'Missing ARM cross compiler')
    for name in ['lib/crt1.o','lib/crti.o','lib/crtn.o','lib/libc.a','include/stdio.h']:
        require((args.musl_prefix/name).is_file(), 'Incomplete ARM musl prefix')
    out.mkdir(parents=True, mode=0o700)
    src = out/'source'
    guard = src/'guardian'
    boot = src/'bootstrap'
    guard.mkdir(parents=True)
    boot.mkdir()
    for name in ['Makefile','gleworks_pid1_reset.c','reset-protocol.h','guard-control.c','guard.h','sys.h']:
        shutil.copyfile(DEVICE/'guardian'/name, guard/name)
    seed = os.environ.get('GW_BUILD_SEED')
    if seed:
        import hashlib
        material = (seed + ':' + str(epoch)).encode()
        identity = ','.join(
            '0x' + hashlib.sha256(material + bytes([i])).hexdigest()[:8] + 'U'
            for i in range(4))
    else:
        identity = ','.join('0x'+secrets.token_hex(4)+'U' for _ in range(4))
    (guard/'reset-token.h').write_text('/* Private paired-build identity; do not publish. */\n#define GW_RESET_TOKEN {'+identity+'}\n')
    for name in ['bootstrap.c','shutdown.c']:
        shutil.copyfile(DEVICE/'bootstrap'/name, boot/name)
    commands = []

    def execute(argv):
        commands.append([str(x) for x in argv])
        result = subprocess.run(commands[-1], capture_output=True)
        with (out/'build.log').open('ab') as log:
            log.write(result.stdout+result.stderr)
        require(result.returncode == 0, 'Build command failed; inspect private build.log')

    cc = args.cross_prefix+'gcc'
    flags = ['-Os','-marm','-march=armv7-a','-mfloat-abi=soft','-Wall','-Wextra','-Werror']
    musl = args.musl_prefix.resolve()
    libgcc = run([cc,'-print-libgcc-file-name'], text=True).stdout.strip()
    for name in ['bootstrap','shutdown']:
        execute([cc,'-std=gnu11',*flags,'-nostdinc','-isystem',musl/'include',
                 '-idirafter',args.linux_headers.resolve(),'-nostdlib','-static',
                 musl/'lib/crt1.o',musl/'lib/crti.o',boot/(name+'.c'),
                 '-Wl,--start-group',musl/'lib/libc.a',libgcc,'-Wl,--end-group',
                 musl/'lib/crtn.o','-o',out/name])
    execute([cc,*flags,'-Wno-unused-function','-fno-builtin','-fno-stack-protector',
             '-fno-pic','-nostdlib','-static','-Wl,-e,_start','-Wl,--build-id=none',
             guard/'guard-control.c','-o',out/'guard-control'])
    execute([args.make,'-C',args.kernel_build.resolve(),
             'KBUILD_SRC='+str(args.kernel_source.resolve()),'-f',args.kernel_source.resolve()/'Makefile',
             'ARCH=arm','CROSS_COMPILE='+args.cross_prefix,'HOSTCC='+args.hostcc,
             'M='+str(guard),'modules'])
    # Rebuild the PW3 UDC with the verified disconnect fix; never edit vendor source.
    usb_source = args.kernel_source.resolve()/'drivers/usb/gadget'
    require(digest(usb_source/'arcotg_udc.c') ==
            '08cb731b6643707cc7015e4a79011db0a46a139b9339acee9d39940b75ae04db',
            'Unexpected PW3 UDC source revision')
    usb_tree = src/'usb'
    usb = usb_tree/'drivers/usb/gadget'
    usb.mkdir(parents=True)
    for path in [usb_source/'arcotg_udc.c', usb_source/'imx_usb_charger.c', *usb_source.glob('*.h')]:
        shutil.copyfile(path, usb/path.name)
    execute(['patch','--batch','--fuzz=0','-d',usb_tree,'-p1','-i',DEVICE/'patches/usb-hotplug.patch'])
    (usb/'Makefile').write_text('obj-m += arcotg_udc.o\n')
    execute([args.make,'-C',args.kernel_build.resolve(),
             'KBUILD_SRC='+str(args.kernel_source.resolve()),'-f',args.kernel_source.resolve()/'Makefile',
             'ARCH=arm','CROSS_COMPILE='+args.cross_prefix,'HOSTCC='+args.hostcc,
             'M='+str(usb),'modules'])
    fixed_udc = usb/'arcotg_udc.ko'
    require(b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in fixed_udc.read_bytes(),
            'Rebuilt UDC kernel ABI mismatch')
    module = guard/'gleworks_pid1_reset.ko'
    require(b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in module.read_bytes(), 'Guardian kernel ABI mismatch')
    shutil.copyfile(module, out/'gleworks_pid1_reset.ko')
    for name in ['bootstrap','shutdown','guard-control']:
        data = (out/name).read_bytes()
        require(data[:5] == b'\x7fELF\x01' and data[18:20] == b'\x28\x00', 'Expected ELF32 ARM: '+name)
        (out/name).chmod(0o700)
    result = run([args.qemu_arm,out/'bootstrap','--check'], text=True)
    require(result.stdout.strip() == 'gleworks-pw3-bootstrap-spike-v1', 'Bootstrap ABI check failed')

    tree = out/'root'
    tree.mkdir()
    unpack(args.rootfs, tree)
    (tree/'sbin/init').unlink()
    (tree/'sbin/init').symlink_to('../bin/busybox')
    for name in ['old-root','boot/gleworks-control','gleworks','dev/input','mnt','etc']:
        (tree/name).mkdir(parents=True, exist_ok=True)

    def copy(source, destination, mode=0o755):
        target = tree/destination
        require(not target.is_symlink(), 'Refusing rootfs symlink: '+destination)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(mode)

    for name in ['guard-control','shutdown','gleworks_pid1_reset.ko']:
        copy(out/name, 'gleworks/'+name, 0o600 if name.endswith('.ko') else 0o700)
    module_names = ['otg/fsl_otg_arc','gadget/arcotg_udc','gadget/g_serial','gadget/g_file_storage']
    if args.variant == 'reader':
        module_names += ['../input/touchscreen/'+x for x in ['cyttsp4_i2c','cyttsp4_core','cyttsp4_mt_b']]
    with tarfile.open(args.modules) as tar:
        for name in module_names:
            rel = str(PurePosixPath(os.path.normpath('lib/modules/'+KERNEL+'/kernel/drivers/usb/'+name+'.ko')))
            member = tar.getmember(rel)
            require(member.isfile() and 0 < member.size <= 4*1024*1024, 'Invalid captured module')
            data = fixed_udc.read_bytes() if name == 'gadget/arcotg_udc' else tar.extractfile(member).read()
            require(b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in data, 'Captured module ABI mismatch')
            target = tree/rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            target.chmod(0o644)
            if 'g_file_storage' in name:
                copy(target, 'gleworks/g_file_storage.ko', 0o600)
            if name in module_names[:2] or 'g_file_storage' in name:
                shutil.copyfile(target, out/(PurePosixPath(name).name+'.ko'))
    for overlay in [ALPINE/'overlay', DEVICE/'overlay']:
        for path in sorted(overlay.rglob('*')):
            require(not path.is_symlink(), 'Source overlay must not contain symlinks')
            if path.is_file():
                copy(path, str(path.relative_to(overlay)), path.stat().st_mode & 0o777)
    if args.variant == 'reader':
        (tree/'etc/inittab').write_text((tree/'etc/inittab').read_text()+(READER/'inittab.fragment').read_text())
        (tree/'etc/gleworks-reader.enabled').touch()
        # No source-side overlay is reused: settings, tokens and book history are private.
        reader = tree/'opt/koreader'
        shutil.copytree(args.reader, reader, symlinks=True)
        for path in reader.rglob('*'):
            require(not path.is_symlink(), 'Reader input must be a clean link-free release bundle')
        for name in ['settings.reader.lua','history.lua','common/libopenlipclua.so']:
            (reader/name).unlink(missing_ok=True)
        for name in ['history','cache','settings','clipboard','ota']:
            if (reader/name).is_dir():
                shutil.rmtree(reader/name)
        for path in (reader/'l10n').iterdir():
            if path.name not in {'vi','en_GB','LICENSE','README.md'}:
                shutil.rmtree(path) if path.is_dir() else path.unlink()
        for path in sorted((READER/'overlay').rglob('*')):
            if path.is_file():
                copy(path, str(path.relative_to(READER/'overlay')), path.stat().st_mode & 0o777)
        compat = tree/'opt/reader-compat'
        compat.mkdir()
        libs = {'ld-linux.so.3':'ld-2.20.so','libc.so.6':'libc-2.20.so','libm.so.6':'libm-2.20.so',
                'libdl.so.2':'libdl-2.20.so','libpthread.so.0':'libpthread-2.20.so','librt.so.1':'librt-2.20.so',
                'libgcc_s.so.1':'libgcc_s.so.1','libresolv.so.2':'libresolv-2.20.so',
                'libnss_files.so.2':'libnss_files-2.20.so','libnss_dns.so.2':'libnss_dns-2.20.so'}
        for name, source in libs.items():
            execute(['debugfs','-R',f'dump /lib/{source} {compat/name}',args.p1_backup])
            require((compat/name).is_file() and (compat/name).stat().st_size > 1000, 'Missing captured library: '+name)
            (compat/name).chmod(0o755)
        for binary, arguments in [('luajit',['-v']),('fbink',['--help']),('luajit',['-e',f'assert(loadfile("{reader}/native.lua"))'])]:
            execute([args.qemu_arm,compat/'ld-linux.so.3','--library-path',str(compat)+':'+str(reader/'libs'),reader/binary,*arguments])
    # Keep one backing FAT mount; these links are intentional runtime paths.
    require(not (tree/'mnt/us').exists(), 'Unexpected minirootfs /mnt/us')
    (tree/'mnt/us').symlink_to('/boot/gleworks-control')
    if args.variant == 'reader':
        (tree/'lib/ld-linux.so.3').symlink_to('/opt/reader-compat/ld-linux.so.3')
    for script in (tree/'etc').glob('gleworks-*.sh'):
        execute(['sh','-n',script])
    image = out/'system.ext3'
    image_mib = image_size_mib(tree)
    # Fixed timestamps and filesystem identity make two builds of the same
    # source comparable byte-for-byte.
    for path in [tree]+sorted(tree.rglob('*')):
        os.utime(path, (epoch, epoch), follow_symlinks=False)
    with image.open('xb') as stream:
        stream.truncate(image_mib*1024*1024)
    execute(['mkfs.ext3','-q','-F','-b','1024','-I','128','-O',
             'none,has_journal,filetype,sparse_super,large_file','-L','AlpinePW3',
             '-m','0','-U','00000000-0000-4000-8000-000000000001',
             '-E','hash_seed=00000000-0000-4000-8000-000000000002',
             '-d',tree,image])
    # Data ordering is cheaper than full data journaling but avoids the old
    # kernel's writeback default.  On an unrecoverable filesystem error, stop
    # writing rather than continuing with a corrupt root.
    execute(['tune2fs','-o','journal_data_ordered','-e','remount-ro',image])
    paths = [tree]+sorted(tree.rglob('*'))
    commands_fs = ['cd /dev','mknod console c 5 1','mknod null c 1 3','cd /',
                   'set_inode_field /dev/console mode 020600','set_inode_field /dev/null mode 020666']
    for path in paths:
        rel = '/' if path == tree else '/'+str(path.relative_to(tree))
        require(not any(c in rel for c in ['"','\n','\r','\\']), 'Unsafe debugfs inode path')
        commands_fs += ['set_inode_field "'+rel+'" uid 0',
                        'set_inode_field "'+rel+'" gid 0',
                        'set_inode_field "'+rel+'" atime 0',
                        'set_inode_field "'+rel+'" ctime 0',
                        'set_inode_field "'+rel+'" mtime 0']
    for name in ['/dev/console','/dev/null','/lost+found']:
        commands_fs += ['set_inode_field '+name+' uid 0',
                        'set_inode_field '+name+' gid 0',
                        'set_inode_field '+name+' atime 0',
                        'set_inode_field '+name+' ctime 0',
                        'set_inode_field '+name+' mtime 0']
    command_file = out/'ownership.debugfs'
    write_atomic(command_file, '\n'.join(commands_fs)+'\n')
    execute(['debugfs','-w','-f',command_file,image])
    clean_image(image)
    for name in ['trial-dispatch.in.sh','install-default-trial.sh','restore-stock-init.sh']:
        shutil.copyfile(DEVICE/'bootstrap'/name, out/('dispatcher.sh' if name == 'trial-dispatch.in.sh' else name))
    launcher_hash = digest(args.return_launcher)
    control = (DEVICE/'bootstrap/return-control.in.conf').read_text().replace('@LAUNCHER_SHA256@', launcher_hash)
    require(launcher_hash in control, 'Return launcher hash rendering failed')
    write_atomic(out/'return-control.conf', control, 0o644)
    payload = ['bootstrap','shutdown','guard-control','gleworks_pid1_reset.ko',
               'fsl_otg_arc.ko','arcotg_udc.ko','g_file_storage.ko','dispatcher.sh',
               'install-default-trial.sh','restore-stock-init.sh','return-control.conf','system.ext3']
    manifest = {name:digest(out/name) for name in payload}
    write_atomic(out/'SHA256SUMS', ''.join(manifest[n]+'  '+n+'\n' for n in payload if n != 'system.ext3'), 0o644)
    manifest['SHA256SUMS'] = digest(out/'SHA256SUMS')
    source_inputs = [ALPINE/'source.json', DEVICE/'profile.json', DEVICE/'stock-kernel.config',
                     Path(__file__).resolve(), TOOLS/'lib/pw3_common.py']
    for directory in [ALPINE/'overlay', DEVICE/'overlay', DEVICE/'bootstrap', DEVICE/'guardian', DEVICE/'patches']:
        source_inputs.extend(p for p in directory.rglob('*') if p.is_file())
    if args.variant == 'reader':
        source_inputs += [READER/'source.json', READER/'inittab.fragment']
        source_inputs.extend(p for p in (READER/'overlay').rglob('*') if p.is_file())
    tool_versions = {}
    for tool in ['mkfs.ext3', 'debugfs', 'e2fsck', 'tune2fs', args.make,
                 args.qemu_arm, cc, args.hostcc]:
        resolved = shutil.which(tool) or tool
        try:
            line = run([resolved, '--version'], text=True).stdout.splitlines()[0]
        except (CommandError, IndexError):
            line = 'unknown'
        tool_versions[tool] = line
    save(out/'candidate.json', {'schema':1,'profile':DEVICE_PROFILE['name'],'variant':args.variant,
         'image_mib':image_mib,
         'files':manifest,'return_launcher_sha256':launcher_hash,
         'inputs':{name:digest(getattr(args,name)) for name in ['rootfs','modules','p1_backup','return_launcher']},
         'source_sha256':{str(p.relative_to(ROOT)):digest(p) for p in sorted(set(source_inputs))},
         'source_date_epoch':epoch,
         'toolchain':{'cc':run([cc,'--version'], text=True).stdout.splitlines()[0],
                      'kernel':KERNEL,
                      'kernel_config_sha256':digest(DEVICE/'stock-kernel.config'),
                      'tools':tool_versions},
         'commands':commands,'physical_verified':False,'private_candidate':True})
    print('Private candidate built and checked:', out)
    print('Physical qualification required; never publish this directory or its image.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['rootfs','modules','p1-backup','return-launcher','kernel-source','kernel-build','musl-prefix','linux-headers']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--variant', choices=['base','reader'], default='base', help='Base Alpine by default; reader adds the KOReader app layer')
    p.add_argument('--reader', type=Path, help='Clean KOReader bundle; reader variant only')
    p.add_argument('--output', type=Path, help='New private candidate directory; defaults under work-dir/build')
    add_work_dir(p)
    p.add_argument('--cross-prefix', required=True, help='ARM soft-float cross tool prefix, ending in -')
    p.add_argument('--hostcc', default='cc')
    p.add_argument('--make', default='make')
    p.add_argument('--qemu-arm', default='qemu-arm')
    args = p.parse_args()
    try:
        build(args)
    except (ValueError, OSError, subprocess.SubprocessError, tarfile.TarError) as error:
        if os.environ.get('PW3_DEBUG'):
            raise
        p.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
