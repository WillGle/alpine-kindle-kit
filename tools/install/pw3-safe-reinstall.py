#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a private PW3 kit; reinstall p4 only from a healthy native boot.

No p1, bootloader, kernel, IDME or raw-disk write commands exist in this tool.
An interrupted write is never retried automatically. See safe-reinstall.md.
"""
from pathlib import Path
import argparse
import contextlib
import fcntl
import importlib.util
import json
import os
import re
import secrets
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/lib'))
from pw3_common import DEVICE_PROFILE, KERNEL, TOKEN, clean_image, digest, require, save, syncdir
from usb_console import Console

PAIRED = ('gleworks/guard-control', 'gleworks/shutdown', 'gleworks/gleworks_pid1_reset.ko')
RUNTIME = ('etc/gleworks-early.sh', 'etc/gleworks-console.sh', 'etc/gleworks-device-prepare.sh',
           'usr/sbin/gw-system', 'usr/sbin/gw-shell', 'usr/sbin/gwctl', 'usr/sbin/gw-appctl')


def maintenance():
    spec = importlib.util.spec_from_file_location('safe_maintenance', ROOT / 'tools/maintenance/kindle-linux.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def image_info(image):
    """Reject a generic/QEMU root even when its ext3 superblock is clean."""
    clean_image(image)
    # debugfs can return success after a failed command. Check each result file.
    with tempfile.TemporaryDirectory(prefix='pw3-image-') as temporary:
        output = Path(temporary)
        hashes = {}
        for index, name in enumerate((*PAIRED, *RUNTIME, 'etc/inittab', 'bin/busybox')):
            target = output / str(index)
            result = subprocess.run(['debugfs', '-R', f'dump /{name} {target}', str(image)],
                                    capture_output=True, text=True, check=True)
            require(target.is_file() and target.stat().st_size > 0,
                    'Not a complete PW3 native image: /' + name + '\n' + result.stderr)
            hashes[name] = digest(target)
            if name == 'etc/inittab':
                require('gleworks-early.sh' in target.read_text(), 'PW3 early boot is not selected')
        for name in ('old-root', 'boot/gleworks-control'):
            result = subprocess.run(['debugfs', '-R', f'stat /{name}', str(image)],
                                    capture_output=True, text=True, check=True)
            require('Type: directory' in result.stdout, 'Missing real image directory: /' + name)
        result = subprocess.run(['debugfs', '-R', 'stat /sbin/init', str(image)],
                                capture_output=True, text=True, check=True)
        require('Type: symlink' in result.stdout and
                'Fast link dest: "../bin/busybox"' in result.stdout,
                'Expected qualified relative BusyBox init link')
    return {'sha256': digest(image), 'bytes': image.stat().st_size,
            'paired': {name: hashes[name] for name in PAIRED},
            'runtime': {name: value for name, value in hashes.items() if name not in PAIRED}}


def verify(kit):
    require(kit.is_dir() and not kit.is_symlink(), 'Expected a real kit directory')
    manifest = json.loads((kit / 'kit.json').read_text())
    require(manifest['schema'] == 1 and manifest['profile'] == DEVICE_PROFILE['name'], 'Unsupported kit')
    for name, wanted in manifest['files'].items():
        path = Path(name)
        require(not path.is_absolute() and '..' not in path.parts, 'Unsafe kit path')
        require(digest(kit / path) == wanted, 'Kit changed: ' + name)
    for name in ('system.ext3', 'fallback.ext3'):
        require(image_info(kit / name) == manifest['images'][name], 'Image metadata changed: ' + name)
    require(manifest['images']['system.ext3']['paired'] == manifest['images']['fallback.ext3']['paired'],
            'Candidate/fallback guardian or shutdown mismatch: rebuild a matching pair')
    return manifest


def prepare(args):
    require(not args.output.exists(), 'Output must be new; retain previous kits')
    images = {name: image_info(path) for name, path in
              [('system.ext3', args.image), ('fallback.ext3', args.fallback)]}
    require(images['system.ext3']['paired'] == images['fallback.ext3']['paired'], 'Unpaired fallback image')
    require(args.p1_backup.stat().st_size == 471859200, 'Expected a complete 450 MiB p1 archive')
    p1_hash = digest(args.p1_backup)
    args.output.mkdir(parents=True, mode=0o700)
    for source, name in [(args.image, 'system.ext3'), (args.fallback, 'fallback.ext3'),
                         (args.p1_backup, 'p1-backup.ext3')]:
        shutil.copyfile(source, args.output / name)
        (args.output / name).chmod(0o600)
    # This kit deliberately has only the p4 automation path. First-install tools
    # remain in the repository with separate stock/physical-recovery gates.
    files = ['tools/install/pw3-safe-reinstall.py', 'tools/maintenance/kindle-linux.py',
             'tools/lib/pw3_common.py', 'tools/lib/usb_console.py',
             'os/alpine/source.json', 'os/devices/pw3/profile.json',
             'AGENTS.md']
    # Keep the complete maintained guidance tree so relative recovery/status
    # links remain usable when the private kit is moved off this checkout.
    files += [str(path.relative_to(ROOT)) for path in sorted((ROOT / 'docs').rglob('*'))
              if path.is_file() and not path.is_symlink()]
    for name in files:
        destination = args.output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / name, destination)
    (args.output / 'README.txt').write_text(
        'PRIVATE PW3 reinstall + fallback kit. Do not publish.\n'
        'Read docs/how-to/safe-reinstall.md before running anything.\n'
        'Requires a healthy native boot first; DOES NOT repair the current p1 failure.\n'
        'p1-backup.ext3 is an archive, not certified bootable and NEVER flashed by this kit.\n'
        'KOReader/Tailscale inside a retained historical image are not the future OS core.\n'
        'Verify: python3 tools/install/pw3-safe-reinstall.py --kit . verify\n'
        'Install: python3 tools/install/pw3-safe-reinstall.py --kit . --session /absolute/new-session install\n'
        'Rollback: python3 tools/install/pw3-safe-reinstall.py --kit . --session /absolute/same-session rollback\n')
    files += ['system.ext3', 'fallback.ext3', 'p1-backup.ext3', 'README.txt']
    manifest = {'schema': 1, 'profile': DEVICE_PROFILE['name'], 'images': images,
                'p1_archive_sha256': p1_hash, 'p1_archive_bootability': 'not certified',
                'device_install_verified': False, 'p1_writes': False,
                'files': {name: digest(args.output / name) for name in sorted(files)}}
    save(args.output / 'kit.json', manifest)
    (args.output / 'SHA256SUMS').write_text(''.join(
        f'{digest(path)}  {path.relative_to(args.output)}\n'
        for path in sorted(args.output.rglob('*')) if path.is_file() and path.name != 'SHA256SUMS'))
    verify(args.output)
    print('Private kit prepared and verified:', args.output)


def native_probe(paired, identity=None, minimum_uptime=100):
    script = f'''set -eu
[ "$(uname -r)" = {KERNEL} ]
[ "$(cut -c 1-6 /proc/usid)" = G090KB ]
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(cat /sys/class/block/mmcblk0p1/start)" = 65536 ]
[ "$(cat /sys/class/block/mmcblk0p1/size)" = 921600 ]
[ "$(cat /sys/class/block/mmcblk0p4/size)" = 6385664 ]
[ "$(cat /sys/class/power_supply/max77696-battery/capacity)" -ge 50 ]
[ -f /run/persistent-boot ]
awk '{{if ($1 < {minimum_uptime}) exit 1}}' /proc/uptime
/gleworks/guard-control status | grep -q '^state=00000000$'
grep -q '^/dev/loop[^ ]* / ext3 rw,' /proc/mounts
'''
    for name, wanted in paired.items():
        script += f'[ -f /{name} ] && [ ! -L /{name} ]\n'
        script += f'[ "$(sha256sum /{name} | awk \'{{print $1}}\')" = {wanted} ]\n'
    if identity:
        script += f'[ "$(sha256sum /proc/usid | awk \'{{print $1}}\')" = {identity} ]\n'
    script += "printf '\\nGW_DEVICE='; sha256sum /proc/usid | awk '{print $1}'\n"
    script += "printf '\\nGW_BOOT='; cat /proc/sys/kernel/random/boot_id\n"
    return script


def wait_native(state, info, seconds=180):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            with contextlib.closing(Console(seconds=5)) as console:
                text = console.run(native_probe({**info['paired'], **info['runtime']},
                                                state['identity']), seconds=15)
            boot = re.search(r'\nGW_BOOT=([0-9a-f-]{36})\r?\n', text)
            require(boot and boot[1] != state['before_boot'], 'No fresh native boot')
            return text
        except (OSError, RuntimeError, ValueError, TimeoutError):
            time.sleep(2)
    raise TimeoutError('No stable native boot within deadline')


def bound_rescue(module, state, seconds=90):
    rescue = module.Rescue(seconds=seconds)
    try:
        rescue.same()
        marker = rescue.control / ('safe-reinstall-' + state['nonce'])
        require(marker.is_file() and not marker.is_symlink() and marker.read_text() == state['identity'] + '\n',
                'Rescue does not belong to this device/session; no writes')
        return rescue
    except BaseException:
        rescue.unmount()
        raise


def replace_root(rescue, image, session, state, label):
    """Keep a host copy before touching the selected root; boot token is last."""
    rescue.same()
    current = rescue.control / 'system.ext3'
    require(current.is_file() and not current.is_symlink(), 'Selected root missing/linked; inspect manually')
    for name in ('boot.enabled', 'boot.once', 'boot.pending', 'boot.repairing'):
        path = rescue.control / name
        require(not path.is_symlink(), 'Symlinked boot marker')
        if path.exists():
            require(path.is_file() and path.read_bytes() == TOKEN, 'Unknown boot marker: ' + name)
            require(label == 'fallback', 'Unclean rescue state; install stopped: ' + name)
    backup = session / (label + '-previous.ext3')
    require(not backup.exists(), 'Backup exists; do not retry this write')
    require(shutil.disk_usage(session).free > current.stat().st_size + 64*1024**2, 'Insufficient host backup space')
    with current.open('rb') as source, backup.open('xb') as target:
        while data := source.read(1024**2):
            rescue.same()
            target.write(data)
        target.flush()
        os.fsync(target.fileno())
    require(digest(backup) == digest(current), 'Host backup readback mismatch')
    state[label + '_backup_sha256'] = digest(backup)
    # Preserve the failure markers and readonly fsck report before rollback.
    state[label + '_markers'] = {p.name: p.read_text() for p in rescue.control.glob('boot.*') if p.is_file()}
    result = subprocess.run(['e2fsck', '-fn', str(backup)], capture_output=True, text=True)
    (session / (label + '-previous.fsck.txt')).write_text(result.stdout + result.stderr)
    state[label + '_previous_fsck_exit'] = result.returncode
    save(session / 'state.json', state)
    require(shutil.disk_usage(rescue.control).free > image.stat().st_size + 64*1024**2,
            'Insufficient p4 staging space; no old images are deleted')
    partial = rescue.control / ('safe-reinstall-' + state['nonce'] + '.partial')
    fd = os.open(partial, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, 'wb') as target, image.open('rb') as source:
        while data := source.read(1024**2):
            rescue.same()
            target.write(data)
        target.flush()
        os.fsync(target.fileno())
    require(digest(partial) == digest(image), 'Staged readback mismatch; previous root retained')
    rescue.same()
    enabled = rescue.control / 'boot.enabled'
    if enabled.exists():
        enabled.unlink()
        syncdir(rescue.control)
    os.replace(partial, current)
    syncdir(rescue.control)
    # Only this journaled fallback may acknowledge a crashed startup, after
    # retaining its image/markers and replacing it with the clean fallback.
    for name in ('boot.enabled', 'boot.once', 'boot.pending', 'boot.repairing'):
        path = rescue.control / name
        if path.exists():
            path.unlink()
    with (rescue.control / 'boot.enabled').open('xb') as stream:
        stream.write(TOKEN)
        stream.flush()
        os.fsync(stream.fileno())
    syncdir(rescue.control)
    require((rescue.control / 'boot.enabled').read_bytes() == TOKEN, 'Boot token readback failed')
    state['phase'] = 'awaiting-' + label
    save(session / 'state.json', state)
    rescue.eject()


def operate(args, manifest):
    require(args.session is not None, '--session is required')
    session = args.session.absolute()
    module = maintenance()
    paired = manifest['images']['fallback.ext3']['paired']
    if args.command == 'install':
        require(not session.exists(), 'New install needs a NEW session; use status/rollback after interruption')
        session.mkdir(parents=True, mode=0o700)
    require(session.is_dir() and not session.is_symlink(), 'Session missing/linked')
    require(session.stat().st_uid == os.getuid() and session.stat().st_mode & 0o077 == 0,
            'Private session must be owned by this user, mode 0700')
    with (session / 'operation.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if args.command == 'install':
            with contextlib.closing(Console(seconds=15)) as console:
                text = console.run(native_probe(paired), seconds=30)
                identity = re.search(r'\nGW_DEVICE=([0-9a-f]{64})\r?\n', text)
                boot = re.search(r'\nGW_BOOT=([0-9a-f-]{36})\r?\n', text)
                require(identity and boot, 'Native identity probe incomplete')
                state = {'schema': 1, 'identity': identity[1], 'before_boot': boot[1],
                         'nonce': secrets.token_hex(16), 'kit_sha256': digest(args.kit / 'kit.json'),
                         'phase': 'entering-rescue', 'initial_probe': text}
                save(session / 'state.json', state)
                path = '/boot/gleworks-control/gleworks-alpine/safe-reinstall-' + state['nonce']
                console.run(f'''set -eu
[ "$(cat /boot/gleworks-control/gleworks-alpine/boot.enabled)" = gleworks-alpine-v1 ]
[ ! -e /boot/gleworks-control/gleworks-alpine/boot.repairing ]
set -C
printf '%s\\n' {state['identity']} > {path}
sync
rm /boot/gleworks-control/gleworks-alpine/boot.enabled
sync
''')
                os.write(console.fd, b'/bin/busybox kill -QUIT 1\n')
                # Shutdown invalidates the USB descriptor; close it without
                # sending the interactive shell's normal exit command.
                os.close(console.fd)
                console.fd = None
            with bound_rescue(module, state) as rescue:
                state['phase'] = 'writing-candidate'
                save(session / 'state.json', state)
                replace_root(rescue, args.kit / 'system.ext3', session, state, 'candidate')
            try:
                state['native_proof'] = wait_native(state, manifest['images']['system.ext3'])
                state['phase'] = 'installed-host-verified'
                save(session / 'state.json', state)
                print('Fresh stable native boot verified. Panel/touch/apps still need physical acceptance.')
                return
            except TimeoutError:
                print('Candidate not verified. Attempting fallback ONCE through bound USB rescue.', flush=True)
        else:
            state = json.loads((session / 'state.json').read_text())
            require(state['kit_sha256'] == digest(args.kit / 'kit.json'), 'Different kit; rollback refused')
            require(re.fullmatch('[0-9a-f]{32}', state['nonce']) and
                    re.fullmatch('[0-9a-f]{64}', state['identity']), 'Invalid session')
            require(state['phase'] in ('entering-rescue', 'writing-candidate', 'awaiting-candidate',
                                      'installed-host-verified'), 'Unknown state or fallback already attempted')
            # Explicit rollback starts in independent rescue. A running native
            # system must enter clean rescue with the maintained rescue command.
        with bound_rescue(module, state, seconds=45) as rescue:
            state['phase'] = 'writing-fallback'
            save(session / 'state.json', state)
            replace_root(rescue, args.kit / 'fallback.ext3', session, state, 'fallback')
        state['fallback_proof'] = wait_native(state, manifest['images']['fallback.ext3'])
        state['phase'] = 'rolled-back-host-verified'
        save(session / 'state.json', state)
        print('Fallback restored; fresh stable native boot verified. No p1 writes performed.')


def main():
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit', type=Path, default=ROOT)
    parser.add_argument('--session', type=Path)
    commands = parser.add_subparsers(dest='command', required=True)
    prepare_parser = commands.add_parser('prepare', help='OFFLINE: create a new private portable kit')
    prepare_parser.add_argument('--image', type=Path, required=True)
    prepare_parser.add_argument('--fallback', type=Path, required=True)
    prepare_parser.add_argument('--p1-backup', type=Path, required=True)
    prepare_parser.add_argument('--output', type=Path, required=True)
    commands.add_parser('verify', help='OFFLINE: verify every payload and both PW3 images')
    commands.add_parser('status', help='OFFLINE: show operation state')
    commands.add_parser('install', help='WRITE/EJECT: healthy native -> rescue -> p4 install; one fallback')
    commands.add_parser('rollback', help='WRITE/EJECT: restore fallback in the SAME session-bound rescue')
    args = parser.parse_args()
    args.kit = args.kit.absolute()
    try:
        if args.command == 'prepare':
            prepare(args)
        elif args.command == 'status':
            require(args.session is not None, '--session is required')
            print((args.session / 'state.json').read_text())
        else:
            manifest = verify(args.kit)
            if args.command == 'verify':
                print('Kit checksums, clean ext3, native runtime and paired fallback: OK')
            else:
                operate(args, manifest)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, 'STOP: ' + str(error) + '\nPreserve kit/session/logs. No blind retry or p1 repair.\n')


if __name__ == '__main__':
    main()
