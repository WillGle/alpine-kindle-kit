#!/usr/bin/env python3
"""Build a private host-only Alpine ext3 trial root; never contacts the Kindle."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import os
import shutil
import subprocess
import tarfile

base = Path(__file__).resolve().parent
linux = base.parent
private = base / '.private'
private.mkdir(mode=0o700, exist_ok=True)
private.chmod(0o700)
tree = private / 'root'
if tree.exists():
    raise SystemExit('Existing root retained; review it before rebuilding.')
archive = linux / 'alpine-boot/alpine-minirootfs-3.24.3-armv7.tar.gz'
assert hashlib.sha256(archive.read_bytes()).hexdigest() == 'dc1ddfaf03ede4be79ad7475490f6b7859dbcbbbbb85150aa1ee013b4bc2f0c1'
tree.mkdir(mode=0o700)

# Create links last so archive paths cannot traverse a previously created link.
links = []
with tarfile.open(archive) as tar:
    for member in tar:
        rel = PurePosixPath(member.name)
        assert not rel.is_absolute() and '..' not in rel.parts
        path = tree.joinpath(*rel.parts)
        if member.isdir():
            path.mkdir(parents=True, exist_ok=True)
        elif member.isfile():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(tar.extractfile(member).read())
        else:
            assert member.issym(), member.name
            links.append((path, member.linkname))
            continue
        path.chmod(member.mode & 0o777)
    for path, target in links:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.symlink_to(target)
tree.chmod(0o755)
# Stock kinit checks this path before chroot; an absolute link resolves outside it.
(tree / 'sbin/init').unlink()
(tree / 'sbin/init').symlink_to('../bin/busybox')

def write(name, text, mode=0o644):
    path = tree / name
    assert not path.is_symlink()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    path.chmod(mode)
    if name.endswith('.sh'):
        subprocess.run(['sh', '-n'], input=text, text=True, check=True)

module_root = 'lib/modules/3.0.35-lab126/'
modules = ['kernel/drivers/usb/otg/fsl_otg_arc.ko',
           'kernel/drivers/usb/gadget/arcotg_udc.ko',
           'kernel/drivers/usb/gadget/g_serial.ko']
inputs = {str(archive.relative_to(linux)): hashlib.sha256(archive.read_bytes()).hexdigest()}
with tarfile.open(linux.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/stock-modules.tar') as tar:
    for name in modules:
        data = tar.extractfile(module_root + name).read()
        assert b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in data
        path = tree / module_root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(0o644)
        inputs[module_root + name] = hashlib.sha256(data).hexdigest()
for name in ['gleworks_pid1_reset.ko', 'guard-control']:
    source = linux / 'pid1-handoff' / name
    path = tree / 'gleworks' / name
    path.parent.mkdir(exist_ok=True)
    shutil.copyfile(source, path)
    path.chmod(0o700 if name == 'guard-control' else 0o600)
    inputs['gleworks/' + name] = hashlib.sha256(path.read_bytes()).hexdigest()

write('etc/inittab', '''::sysinit:/bin/busybox sh /etc/gleworks-early.sh
ttyGS0::respawn:/bin/busybox sh /etc/gleworks-proof.sh
''')
write('etc/gleworks-early.sh', '''#!/bin/sh
set -eu
bb=/bin/busybox
$bb mount -t proc proc /proc
$bb mount -t sysfs sysfs /sys
$bb mount -t tmpfs -o mode=755 tmpfs /dev
$bb mknod /dev/null c 1 3
$bb chmod 666 /dev/null
$bb mknod /dev/console c 5 1
$bb chmod 600 /dev/console
$bb mkdir -p /dev/pts /dev/shm
$bb mount -t devpts devpts /dev/pts
$bb mount -t tmpfs -o mode=1777 tmpfs /dev/shm
$bb mount -t tmpfs -o mode=755 tmpfs /run
$bb mount -t tmpfs -o mode=1777 tmpfs /tmp
# Arm before USB setup: failure below still leaves the stock reset deadline.
if ! $bb grep -q '^gleworks_pid1_reset ' /proc/modules; then
    $bb insmod /gleworks/gleworks_pid1_reset.ko
fi
minor=$($bb awk '$2=="gleworks_pid1_reset" {print $1}' /proc/misc)
case "$minor" in ''|*[!0-9]*) exit 1;; esac
$bb mknod /dev/gleworks_pid1_reset c 10 "$minor"
$bb chmod 600 /dev/gleworks_pid1_reset
/gleworks/guard-control status > /run/guardian.log
if ! $bb grep -q '^state=00000002$' /run/guardian.log; then
    /gleworks/guard-control arm > /run/guardian.log
fi
module=/lib/modules/3.0.35-lab126/kernel/drivers/usb
$bb insmod "$module/otg/fsl_otg_arc.ko"
$bb insmod "$module/gadget/arcotg_udc.ko"
$bb insmod "$module/gadget/g_serial.ko" use_acm=1 n_ports=1
major=$($bb awk '$2=="ttyGS" {print $1}' /proc/devices)
case "$major" in ''|*[!0-9]*) exit 1;; esac
$bb mknod /dev/ttyGS0 c "$major" 0
$bb chmod 600 /dev/ttyGS0
$bb stty -F /dev/ttyGS0 raw -echo
echo ready > /run/usb-ready
''', 0o755)
write('etc/gleworks-proof.sh', '''#!/bin/sh
bb=/bin/busybox
[ -f /run/usb-ready ] || { $bb sleep 2; exit 1; }
while IFS= read -r challenge; do
    [ "${#challenge}" = 32 ] || continue
    case "$challenge" in *[!0-9a-f]*) continue;; esac
    [ "$($bb cat /etc/alpine-release)" = 3.24.3 ] || continue
    [ "$($bb readlink /proc/1/exe)" = /bin/busybox ] || continue
    printf 'GLEWORKS_ALPINE_NATIVE_ROOT release=3.24.3 kernel=%s pid1=%s challenge=%s\\n' "$($bb uname -r)" "$($bb readlink /proc/1/exe)" "$challenge"
done
''', 0o755)
write('etc/fstab', '# First cold-start candidate: root readonly, mutable trial state in RAM.\n')
for name in ['proc', 'sys', 'dev', 'run', 'tmp', 'old-root', 'boot/gleworks-control']:
    (tree / name).mkdir(parents=True, exist_ok=True)

image = private / 'alpine-native-root-HOST-ONLY.ext3'
with image.open('wb') as stream:
    stream.truncate(64 * 1024 * 1024)
subprocess.run(['mkfs.ext3', '-q', '-F', '-b', '1024', '-I', '128',
                '-O', 'none,has_journal,filetype,sparse_super,large_file',
                '-L', 'AlpineTrial', '-m', '0', '-d', str(tree), str(image)], check=True)
# mke2fs -d preserves host UID/GID: make every image inode root-owned explicitly.
paths = [tree] + sorted(tree.rglob('*'))
commands = []
# Stock kernel has no devtmpfs; init needs a console before sysinit mounts /dev.
commands.extend(['cd /dev', 'mknod console c 5 1', 'mknod null c 1 3', 'cd /',
                 'set_inode_field /dev/console mode 020600',
                 'set_inode_field /dev/null mode 020666',
                 'set_inode_field /dev/console uid 0',
                 'set_inode_field /dev/console gid 0',
                 'set_inode_field /dev/null uid 0',
                 'set_inode_field /dev/null gid 0'])
for path in paths:
    rel = '/' + str(path.relative_to(tree)) if path != tree else '/'
    assert all(c not in rel for c in ['"', '\n', '\\', ' '])
    commands.extend([f'set_inode_field {rel} uid 0', f'set_inode_field {rel} gid 0'])
script = private / 'root-ownership.debugfs'
script.write_text('\n'.join(commands) + '\n')
result = subprocess.run(['debugfs', '-w', '-f', str(script), str(image)], capture_output=True, text=True)
(base / 'ownership.log').write_text(result.stdout + result.stderr)
assert result.returncode == 0 and 'not found' not in result.stderr.lower()
result = subprocess.run(['e2fsck', '-fn', str(image)], capture_output=True, text=True)
(base / 'filesystem-check.log').write_text(result.stdout + result.stderr)
assert result.returncode == 0, result.stdout + result.stderr
image.chmod(0o600)
review = {'scope': 'HOST ONLY; no device access or installer',
          'rootfs': 'Alpine3.24.3 armv7 full minirootfs; direct BusyBox PID1',
          'bytes': image.stat().st_size, 'sha256': hashlib.sha256(image.read_bytes()).hexdigest(),
          'input_sha256': inputs, 'image_inodes_root_owned': True,
          'readonly_root_required': True, 'trial_reset_seconds': 90,
          'hardware_coldboot_verified': False, 'early_USB_verified': False,
          'pre_guardian_failure_recovery_verified': False,
          'cable_rescue_after_rootfs_failure_verified': False,
          'install_decision': 'NO-GO until independent cable recovery',
          'amazon_userspace_started_by_image': False}
(base / 'host-review.json').write_text(json.dumps(review, indent=2) + '\n')
print('Full Alpine trial root built; filesystem check passed. HOST ONLY; installation NO-GO.')
