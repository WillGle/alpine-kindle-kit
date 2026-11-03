#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a private, unqualified reader-OS trial from an existing paired root."""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'lib'))
from pw3_common import DEVICE_PROFILE, ROOT, clean_image, digest, image_size_mib, require, run, save, write_atomic


def main():
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH', '0'))
    os.environ['SOURCE_DATE_EPOCH'] = str(epoch)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True,
                        help='Prepared copy of this candidate root with OpenRC installed')
    parser.add_argument('--service', type=Path, required=True)
    parser.add_argument('--shell', type=Path, required=True)
    parser.add_argument('--agent', type=Path, required=True)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--appctl', type=Path,
                        default=ROOT / 'os/system' / 'gw-appctl')
    parser.add_argument('--wifi-modules', type=Path, help='Private captured stock module archive')
    parser.add_argument('--stock-p1', type=Path, help='Private stock p1 backup for Wi-Fi firmware')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        build(args, epoch)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')


def build(args, epoch):
    os.umask(0o077)
    out = args.output.absolute()
    source = args.root.resolve()
    require(not out.exists(), 'Output must be a new directory')
    require(source.is_dir() and not args.root.is_symlink(), 'Expected a prepared root directory')
    require(not out.resolve().is_relative_to(source), 'Output cannot be inside its source root')
    require(re.fullmatch(r'[/A-Za-z0-9_.+-]+', str(out)) is not None,
            'Output path contains characters unsupported by debugfs')
    candidate = json.loads((args.candidate / 'candidate.json').read_text())
    require(candidate.get('profile') == DEVICE_PROFILE['name'] and candidate.get('variant') == 'reader',
            'Expected the paired PW3 reader candidate')
    files = candidate['files']
    require(bool(args.wifi_modules) == bool(args.stock_p1), 'Supply both Wi-Fi modules and stock p1, or neither')
    for name in ['guard-control', 'shutdown', 'gleworks_pid1_reset.ko']:
        require(digest(source / 'gleworks' / name) == files[name], 'Paired identity mismatch: ' + name)
    for name in ['arcotg_udc', 'fsl_otg_arc', 'g_file_storage']:
        path = source / 'lib/modules' / DEVICE_PROFILE['kernel'] / 'kernel/drivers/usb'
        path = path / ('otg' if name == 'fsl_otg_arc' else 'gadget') / (name + '.ko')
        require(digest(path) == files[name + '.ko'], 'USB module mismatch: ' + name)
    require((source / 'sbin/openrc').is_file(), 'OpenRC must already be installed in the prepared root')
    for name, binary in [('service', args.service), ('shell', args.shell),
                         ('client', args.client), ('appctl', args.appctl)]:
        require(binary.is_file() and not binary.is_symlink(),
                'Missing binary: ' + name + ' (' + str(binary) + ')')
        data = binary.read_bytes()
        require(data[:5] == b'\x7fELF\x01' and data[18:20] == b'\x28\x00',
                'Expected ELF32 ARM binary: ' + name)
        if name == 'service':
            require(b'GW_LIFECYCLE_SPIKE=1' in data, 'Service lacks trial lifecycle flag')
            require(b'GW_ENABLE_HW_TRIAL=1' in data,
                    'Service lacks the explicit hardware trial flag')
            require(b'GW_POWER_LIGHT_ACCEPTED=1' not in data,
                    'Accepted frontlight path is not allowed in a trial image')
            require(b'GW_POWER_SUSPEND_ACCEPTED=1' not in data,
                    'Accepted suspend path is not allowed in a trial image')
            require(b'GW_WIFI_ACCEPTED=1' not in data, 'Wi-Fi has not been accepted')
        if name == 'shell':
            require(b'GW_UI_TRIAL_FLAGS=1' in data, 'Shell lacks trial display, touch or lifecycle flag')
    for tool in ['mkfs.ext3', 'debugfs', 'e2fsck', 'tune2fs']:
        require(shutil.which(tool), 'Missing host tool: ' + tool)

    out.mkdir(parents=True, mode=0o700)
    tree = out / 'root'
    shutil.copytree(source, tree, symlinks=True)

    def install(src, rel, mode):
        target = tree / rel
        require(not target.is_symlink(), 'Refusing to overwrite symlink: ' + str(rel))
        target.parent.mkdir(parents=True, exist_ok=True)
        require(not any(parent.is_symlink() for parent in target.parents if parent != tree.parent),
                'Refusing symlink in destination path: ' + str(rel))
        shutil.copyfile(src, target)
        target.chmod(mode)

    system = ROOT / 'os/system'
    device = ROOT / 'os/devices/pw3/overlay'
    reader = ROOT / 'apps/koreader/overlay'
    for src, rel, mode in [
        (args.service, 'usr/sbin/gw-system', 0o755),
        (args.shell, 'usr/sbin/gw-shell', 0o755),
        (args.client, 'usr/sbin/gwctl', 0o755),
        (args.appctl, 'usr/sbin/gw-appctl', 0o755),
        (args.agent, 'usr/sbin/gw-agent', 0o755),
        (device / 'usr/sbin/gw-tailscale', 'usr/sbin/gw-tailscale', 0o755),
        (device / 'usr/sbin/gw-lockscreen-import', 'usr/sbin/gw-lockscreen-import', 0o755),
        (device / 'usr/sbin/gw-vault-pull', 'usr/sbin/gw-vault-pull', 0o755),
        (device / 'usr/lib/gleworks/convert-lockscreen.lua',
         'usr/lib/gleworks/convert-lockscreen.lua', 0o644),
        (device / 'etc/gleworks-early.sh', 'etc/gleworks-early.sh', 0o755),
        (system / 'app-keys' / 'release.pem', 'etc/gleworks/app-keys/release.pem', 0o644),
        (system / 'gw-system.openrc', 'etc/init.d/gw-system', 0o755),
        (system / 'gw-agent.openrc', 'etc/init.d/gw-agent', 0o755),
        (system / 'gleworks-openrc.sh', 'etc/gleworks-openrc.sh', 0o755),
        (system / 'gleworks-device-prepare.sh', 'etc/gleworks-device-prepare.sh', 0o755),
        (system / 'gleworks-hardware-prepare.sh', 'etc/gleworks-hardware-prepare.sh', 0o755),
        (reader / 'etc/gleworks-managed-reader.sh', 'etc/gleworks-managed-reader.sh', 0o755),
        (reader / 'opt/koreader/native.lua', 'opt/koreader/native.lua', 0o644),
        (reader / 'opt/koreader/plugins/gw-home.koplugin/_meta.lua',
         'opt/koreader/plugins/gw-home.koplugin/_meta.lua', 0o644),
        (reader / 'opt/koreader/plugins/gw-home.koplugin/main.lua',
         'opt/koreader/plugins/gw-home.koplugin/main.lua', 0o644),
    ]:
        install(src, rel, mode)

    if args.wifi_modules:
        require(digest(args.wifi_modules) == candidate['inputs']['modules'], 'Captured module archive differs from paired candidate')
        require(digest(args.stock_p1) == candidate['inputs']['p1_backup'], 'Stock p1 backup differs from paired candidate')
        for path in ['sbin/wpa_supplicant', 'sbin/wpa_cli', 'usr/share/udhcpc/default.script']:
            require((tree / path).is_file(), 'Prepared root lacks Wi-Fi dependency: ' + path)
        base = 'lib/modules/' + DEVICE_PROFILE['kernel'] + '/kernel/compat-wireless/'
        modules = ['net/wireless/cfg80211.ko', 'drivers/net/wireless/ath/ath.ko',
                   'drivers/net/wireless/ath/ath6kl/ath6kl_sdio.ko']
        with tarfile.open(args.wifi_modules) as archive:
            for name in modules:
                member = archive.getmember(base + name)
                require(member.isfile() and 0 < member.size <= 4 * 1024 * 1024, 'Invalid Wi-Fi module')
                data = archive.extractfile(member).read()
                require(b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in data,
                        'Wi-Fi module ABI mismatch')
                target = tree / base / name
                target.parent.mkdir(parents=True, exist_ok=True)
                require(not target.exists() and not target.is_symlink(), 'Wi-Fi module already present')
                target.write_bytes(data)
                target.chmod(0o644)
        firmware = 'opt/ar6k/target/AR6003/hw2.1.1/bin/'
        for name in ['athwlan.bin', 'otp.bin', 'bdata.bin', 'AR6803_default.bin', 'fw-4.bin']:
            target = tree / firmware / name
            target.parent.mkdir(parents=True, exist_ok=True)
            require(not target.exists() and not target.is_symlink(), 'Firmware target already present')
            run(['debugfs', '-R', 'dump /' + firmware + name + ' ' + str(target), args.stock_p1])
            require(target.is_file() and target.stat().st_size > 0, 'Missing private firmware: ' + name)
            target.chmod(0o644)
        install(system / 'gleworks-firmware-hotplug.sh', 'sbin/hotplug', 0o755)
        install(system / 'gleworks-wifi.sh', 'etc/gleworks-wifi.sh', 0o755)

    inittab = tree / 'etc/inittab'
    original = inittab.read_text()
    reader_entry = (ROOT / 'apps/koreader/inittab.fragment').read_text()
    require(original.count(reader_entry) == 1, 'Unexpected reader inittab; refusing to alter it')
    inittab.write_text(original.replace(reader_entry, (system / 'inittab.fragment').read_text()))
    runlevel = tree / 'etc/runlevels/default'
    require(runlevel.is_dir() and not runlevel.is_symlink(), 'Missing OpenRC default runlevel')
    link = runlevel / 'gw-system'
    require(not link.exists() and not link.is_symlink(), 'Existing gw-system runlevel entry')
    link.symlink_to('../../init.d/gw-system')
    agent_link = runlevel / 'gw-agent'
    require(not agent_link.exists() and not agent_link.is_symlink(),
            'Existing gw-agent runlevel entry')
    agent_link.symlink_to('../../init.d/gw-agent')

    image = out / 'system.ext3'
    image_mib = image_size_mib(tree)
    for path in [tree] + sorted(tree.rglob('*')):
        os.utime(path, (epoch, epoch), follow_symlinks=False)
    with image.open('xb') as stream:
        stream.truncate(image_mib * 1024 * 1024)
    run(['mkfs.ext3', '-q', '-F', '-b', '1024', '-I', '128', '-O',
         'none,has_journal,filetype,sparse_super,large_file', '-L', 'AlpinePW3',
         '-m', '0', '-U', '00000000-0000-4000-8000-000000000001',
         '-E', 'hash_seed=00000000-0000-4000-8000-000000000002',
         '-d', tree, image])
    run(['tune2fs', '-o', 'journal_data_ordered', '-e', 'remount-ro', image])
    commands = ['cd /dev', 'mknod console c 5 1', 'mknod null c 1 3', 'cd /',
                'set_inode_field /dev/console mode 020600', 'set_inode_field /dev/null mode 020666']
    for path in [tree] + sorted(tree.rglob('*')):
        rel = '/' if path == tree else '/' + str(path.relative_to(tree))
        require(not any(c in rel for c in ['"', '\n', '\r', '\\']), 'Unsafe debugfs inode path')
        commands += ['set_inode_field "' + rel + '" uid 0', 'set_inode_field "' + rel + '" gid 0']
    for name in ['/dev/console', '/dev/null']:
        commands += ['set_inode_field ' + name + ' uid 0', 'set_inode_field ' + name + ' gid 0']
    script = out / 'ownership.debugfs'
    write_atomic(script, '\n'.join(commands) + '\n')
    run(['debugfs', '-w', '-f', script, image])
    clean_image(image)
    save(out / 'trial.json', {
        'profile': DEVICE_PROFILE['name'], 'kind': 'unqualified-reader-os-trial',
        'paired_candidate': str(args.candidate), 'paired_bootstrap_sha256': files['bootstrap'],
        'paired_guardian_sha256': files['gleworks_pid1_reset.ko'],
        'image_mib': image_mib,
        'source_date_epoch': epoch,
        'system_sha256': digest(image),
        'binaries': {name: digest(getattr(args, name)) for name in
                     ['service', 'shell', 'client', 'appctl']},
        'agent_sha256': digest(args.agent),
        'signed_apps': {'trust_key': 'etc/gleworks/app-keys/release.pem',
                        'installer': 'usr/sbin/gw-appctl',
                        'usb_drop': 'gleworks/apps/<name>.gwapp(.sig)'},
        'physical_verified': False, 'wifi_trial_included': bool(args.wifi_modules),
        'data_migration_verified': False,
        'warning': 'Private experimental image; do not deploy before preserving live reader data and rescue proof.',
    })
    print('Private unqualified trial:', out)
    print('No device was changed. Live reader data migration and hardware validation remain required.')


if __name__ == '__main__':
    main()
