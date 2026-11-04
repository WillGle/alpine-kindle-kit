#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Boot a disposable copy of the Alpine core ext3 in QEMU; never deploy it."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import lzma
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]


def run(argv, **kwargs):
    return subprocess.run([str(x) for x in argv], check=True, **kwargs)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def metrics(text):
    if 'GW_CORE_VM_METRICS_START' not in text:
        return None
    block = text.split('GW_CORE_VM_METRICS_START', 1)[1].split('GW_CORE_VM_METRICS_END', 1)[0]
    memory = {key: int(value) for key, value in re.findall(r'^(\w+):\s+(\d+) kB', block, re.M)}
    cpu = [[int(x) for x in row.split()] for row in re.findall(r'^cpu\s+(.+)$', block, re.M)]
    used = memory['MemTotal'] - sum(memory[x] for x in ('MemFree', 'Buffers', 'Cached'))
    result = {'ram_used_excluding_buffers_cache_kib': used,
              'mem_available_kib': memory.get('MemAvailable'), 'memory_kib': memory,
              'scope': '512 MiB ARM virt guest; full shell with stub display/touch, VM input helper, no network/PW3 drivers'}
    if len(cpu) == 2:
        delta = [b - a for a, b in zip(cpu[0][:8], cpu[1][:8])]
        total = sum(delta)
        result.update({'cpu_sample_seconds': 10, 'cpu_delta_ticks': delta,
                       'cpu_busy_percent': 100 * (total - delta[3] - delta[4]) / total if total else None})
    fs = re.search(r'^/dev/vda\s+(\d+)\s+(\d+)\s+(\d+)', block, re.M)
    if fs:
        result['guest_df_kib'] = dict(zip(('total', 'used', 'available'), map(int, fs.groups())))
    return result


def cpio(root, output):
    """newc permits static console nodes without requiring host mknod/root."""
    data = bytearray()

    def add(name, mode, content=b'', major=0, minor=0):
        encoded = name.encode() + b'\0'
        fields = (1, mode, 0, 0, 1, 0, len(content), 0, 0, major, minor, len(encoded), 0)
        data.extend(b'070701' + ''.join(f'{x:08x}' for x in fields).encode())
        data.extend(encoded)
        data.extend(b'\0' * (-len(data) % 4))
        data.extend(content)
        data.extend(b'\0' * (-len(data) % 4))

    for path in sorted(root.rglob('*')):
        name = path.relative_to(root).as_posix()
        mode = path.lstat().st_mode
        content = os.readlink(path).encode() if path.is_symlink() else path.read_bytes() if path.is_file() else b''
        add(name, mode, content)
    add('dev/console', stat.S_IFCHR | 0o600, major=5, minor=1)
    add('dev/null', stat.S_IFCHR | 0o666, major=1, minor=3)
    add('TRAILER!!!', 0)
    output.write_bytes(gzip.compress(data, mtime=0))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--core', required=True, type=Path)
    p.add_argument('--kernel', required=True, type=Path)
    p.add_argument('--modules', required=True, type=Path)
    p.add_argument('--test-binaries', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--external-apps', type=Path, help='Optional private VM-signed KOReader/Tailscale exports')
    p.add_argument('--abi-probe', type=Path, help='Optional ARM float-call regression executable')
    p.add_argument('--recovery', action='store_true', help='Verify explicit VM console recovery without the service/UI')
    p.add_argument('--qemu', default=os.environ.get('QEMU_SYSTEM_ARM', 'qemu-system-arm'))
    p.add_argument('--timeout', type=int, default=120)
    args = p.parse_args()
    out = args.output.absolute()
    if out.exists():
        p.error('output must be new')
    out.mkdir(parents=True)
    image = args.core / 'core.ext3'
    original_sha = sha(image)
    disk = out / 'check.ext3'
    shutil.copyfile(image, disk)
    if args.external_apps:
        # The measured core stays 64 MiB. Only its disposable app-test disk
        # needs space for real optional bundles and their extracted payloads.
        with disk.open('r+b') as stream:
            stream.truncate(256 * 1024 * 1024)
        run(['e2fsck', '-fn', disk], capture_output=True)
        run(['resize2fs', disk], capture_output=True)
    fixtures = out / 'fixtures'
    fixtures.mkdir()
    private = REPO / 'os/system/tests/keys/test-release.key'
    public = REPO / 'os/system/tests/keys/test-release.pem'
    if sha(args.test_binaries / 'release.pem') != sha(public):
        p.error('test binaries must use the fixture public key; never use a production signing key')

    for name, app_id, version, body in [
        ('probe-1', 'qemu-probe', '1.0',
         'echo v1 > "$GW_APP_DATA/version"\n'
         'if [ ! -f /run/rollback ]; then echo $$ > /run/probe.pid; exec sleep 60; fi\n'),
        ('probe-2', 'qemu-probe', '2.0', 'echo v2 > "$GW_APP_DATA/version"\nexit 23\n'),
        ('reader', 'koreader', '1.0', 'touch /run/reader-fixture-ran\n'),
        ('crash', 'qemu-crash', '1.0', 'kill -SEGV $$\n'),
    ]:
        source = out / name
        (source / 'payload').mkdir(parents=True)
        (source / 'manifest').write_text(
            f'id={app_id}\nversion={version}\nentrypoint=payload/run\n'
            'arch=armv7\nprofile=pw3-0kb-5.16.2.1.1\nmin_os=0.1\nformats=\npermissions=\n')
        entry = source / 'payload/run'
        entry.write_text('#!/bin/sh\nset -eu\n' + body)
        entry.chmod(0o755)
        run([sys.executable, REPO / 'tools/build/gwapp.py', 'build', '--source', source,
             '--out', fixtures / (name + '.gwapp'), '--key', private], stdout=subprocess.DEVNULL)
    shutil.copyfile(fixtures / 'probe-2.gwapp', fixtures / 'broken.gwapp')
    with (fixtures / 'broken.gwapp').open('ab') as stream:
        stream.write(b'tampered')
    shutil.copyfile(fixtures / 'probe-2.gwapp.sig', fixtures / 'broken.gwapp.sig')

    # Only the cloned VM disk gets a fixture shell/key/binaries and test data.
    commands = ['mkdir /vm']
    files = {f'/usr/sbin/{n}': args.test_binaries / n for n in ('gw-system', 'gw-appctl', 'gwctl')}
    files['/usr/sbin/gw-shell'] = args.test_binaries / 'vm-shell'
    files['/vm/gw-shell-real'] = args.core / 'root/usr/sbin/gw-shell'
    files['/etc/gleworks/app-keys/release.pem'] = public
    files.update({f'/vm/{x.name}': x for x in fixtures.iterdir()})
    if args.external_apps:
        for name in ('koreader.gwapp', 'koreader.gwapp.sig', 'tailscale.gwapp', 'tailscale.gwapp.sig'):
            files['/vm/' + name] = args.external_apps / name
    init = out / 'inittab'
    init.write_text((args.core / 'root/etc/inittab').read_text() + '::once:/bin/sh /vm/check.sh\n')
    files['/etc/inittab'] = init
    vm_boot = out / 'gleworks-core-boot.sh'
    vm_boot.write_text((args.core / 'root/etc/gleworks-core-boot.sh').read_text() + '''
if [ ! -f /run/core-recovery ]; then
/usr/sbin/gw-shell --input &
for n in $(seq 1 50); do
    [ ! -f /run/input-fixture-ready ] || break
    sleep .1
done
[ -f /run/input-fixture-ready ]
sleep .2
# The core UI is built with EPDC/touch disabled. Supply only the harmless
# openable framebuffer stub it requires; no simulated EPDC acceptance.
ln -s /dev/null /dev/fb0
fi
''')
    files['/etc/gleworks-core-boot.sh'] = vm_boot
    guest = out / 'check.sh'
    guest.write_text('''#!/bin/sh
set -eu
fail() { echo "GW_CORE_VM_FAIL $*"; cat /run/gw-system.log; poweroff -f; }
for n in $(seq 1 160); do
    [ ! -f /run/check-failed ] || fail shell-check
    if [ -f /run/probe.pid ] && [ ! -f /run/app-stopped ]; then
        pid=$(cat /run/probe.pid)
        touch /run/app-stopped
        kill -TERM "$pid" || fail app-stop
    fi
    [ ! -f /run/check-complete ] || break
    sleep .2
done
[ -f /run/check-complete ] || fail deadline
sleep 3
pidof gw-shell-real >/dev/null || fail real-shell-loop
echo GW_CORE_CHECK_PASS real-shell-idle
''' + ('''
for app in koreader tailscale; do
    gw-appctl install /vm/$app.gwapp || fail external-install-$app
    echo GW_CORE_CHECK_PASS real-$app-install
    GW_APP_DATA=/var/lib/gleworks/appdata/$app /var/lib/gleworks/apps/$app/active/payload/run --version || fail external-smoke-$app
    echo GW_CORE_CHECK_PASS real-$app-version
    gw-appctl remove "$app" || fail external-remove-$app
    echo GW_CORE_CHECK_PASS real-$app-remove
done
rm /vm/*.gwapp /vm/*.sig
sleep 3
''' if args.external_apps else '') + '''
echo GW_CORE_VM_METRICS_START
cat /proc/meminfo
df -k /
cat /proc/stat | head -1
sleep 10
cat /proc/stat | head -1
ps
echo GW_CORE_VM_METRICS_END
sync
echo GW_CORE_VM_PASS
poweroff -f
''')
    if args.recovery:
        guest.write_text('''#!/bin/sh
set -eu
[ -f /run/core-recovery ]
sleep 2
if pidof gw-system; then echo GW_CORE_VM_FAIL recovery-service-active; poweroff -f; exit 1; fi
/bin/sh -c 'test -x /usr/sbin/gw-appctl && echo GW_CORE_CHECK_PASS recovery-shell-command'
echo GW_CORE_VM_PASS
poweroff -f
''')
    if args.abi_probe and not args.recovery:
        files['/vm/abi-probe'] = args.abi_probe
        guest.write_text(guest.read_text().replace('echo GW_CORE_VM_METRICS_START',
            '/vm/abi-probe || fail float-abi\necho GW_CORE_CHECK_PASS float-abi-roundtrip\n'
            'echo GW_CORE_VM_METRICS_START'))
    files['/vm/check.sh'] = guest
    for target, source in files.items():
        # debugfs has no shell quoting; all paths are generated or constrained.
        if any(c.isspace() for c in str(source.absolute())):
            p.error('input/output paths must not contain whitespace')
        commands += [f'rm {target}', f'write {source.absolute()} {target}',
                     f'set_inode_field {target} uid 0', f'set_inode_field {target} gid 0',
                     f'set_inode_field {target} mode 0100{755 if target.startswith("/usr/sbin/") or target in ("/vm/gw-shell-real", "/vm/abi-probe") else 644}']
    cmd = out / 'overlay.debugfs'
    cmd.write_text('\n'.join(commands) + '\n')
    result = run(['debugfs', '-w', '-f', cmd, disk], capture_output=True, text=True)
    (out / 'overlay.log').write_text(result.stdout + result.stderr)

    root = out / 'initramfs'
    for name in ('bin', 'lib', 'dev', 'proc', 'sys', 'newroot', 'modules'):
        (root / name).mkdir(parents=True, exist_ok=True)
    for name in ('bin/busybox', 'lib/ld-musl-armhf.so.1'):
        shutil.copyfile(args.core / 'root' / name, root / name)
        (root / name).chmod(0o755)
    # Matching Debian modules belong to the VM boot adapter, never the PW3 core.
    module_names = ['virtio_mmio', 'virtio_blk', 'crc16', 'crc32c_generic', 'mbcache', 'jbd2', 'ext4',
                    'evdev', 'uinput']
    for name in module_names:
        matches = list(args.modules.rglob(name + '.ko.xz'))
        if len(matches) != 1:
            p.error('missing or ambiguous kernel module: ' + name)
        (root / 'modules' / (name + '.ko')).write_bytes(lzma.decompress(matches[0].read_bytes()))
    boot = root / 'init'
    boot.write_text('''#!/bin/busybox sh
set -eu
bb=/bin/busybox
$bb mount -t proc proc /proc
$bb mount -t sysfs sysfs /sys
$bb mount -t devtmpfs devtmpfs /dev
''' + ''.join(f'$bb insmod /modules/{n}.ko\n' for n in module_names) + '''
for n in 1 2 3 4 5; do [ ! -b /dev/vda ] || break; $bb sleep 1; done
$bb mount -t ext3 /dev/vda /newroot
$bb umount /proc
$bb umount /sys
$bb umount /dev
exec $bb switch_root /newroot /sbin/init
''')
    boot.chmod(0o755)
    initrd = out / 'boot.cpio.gz'
    cpio(root, initrd)
    argv = [args.qemu, '-M', 'virt', '-cpu', 'cortex-a15', '-m', '512M', '-nographic',
            '-nic', 'none', '-no-reboot', '-kernel', str(args.kernel.absolute()),
            '-initrd', str(initrd), '-drive', f'file={disk},format=raw,if=none,id=core',
            '-device', 'virtio-blk-device,drive=core',
            '-append', 'console=ttyAMA0 rdinit=/init panic=-1 quiet' +
                       (' gleworks.recovery=1' if args.recovery else '')]
    (out / 'command.json').write_text(json.dumps(argv, indent=2) + '\n')
    with (out / 'boot.log').open('w') as log:
        try:
            result = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
    text = (out / 'boot.log').read_text(errors='replace')
    good = code == 0 and 'GW_CORE_VM_PASS' in text and 'GW_CORE_CHECK_FAIL' not in text and 'GW_CORE_VM_FAIL' not in text
    report = {'passed': good, 'qemu_exit': code, 'core_sha256_before': original_sha,
              'core_sha256_after': sha(image), 'kernel_sha256': sha(args.kernel),
              'checks': [x.strip() for x in text.splitlines() if 'GW_CORE_CHECK_' in x],
              'metrics': metrics(text),
              'scope': 'ARM VM console recovery' if args.recovery else
                       'ARM VM ext3 boot and signed fixture apps; no PW3 hardware or OS A/B acceptance'}
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(text[-14000:])
    if not good or report['core_sha256_after'] != original_sha:
        raise SystemExit(f'QEMU CORE CHECK FAILED: {out}')
    print(f'QEMU CORE CHECK PASSED: {out}')


if __name__ == '__main__':
    main()
