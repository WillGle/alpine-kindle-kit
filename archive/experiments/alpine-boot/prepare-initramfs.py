#!/usr/bin/env python3
"""Select pinned Alpine files into a host-only, storage-free initramfs input."""
from pathlib import Path
import hashlib
import json
import subprocess
import tarfile

base=Path(__file__).resolve().parent
archive=base/'alpine-minirootfs-3.24.3-armv7.tar.gz'
assert hashlib.sha256(archive.read_bytes()).hexdigest()=='dc1ddfaf03ede4be79ad7475490f6b7859dbcbbbbb85150aa1ee013b4bc2f0c1'
selected=base/'selected'
selected.mkdir(exist_ok=True)
with tarfile.open(archive,'r:gz') as tar:
    members={m.name.removeprefix('./'):m for m in tar.getmembers()}
    for name in ['bin/busybox','lib/ld-musl-armhf.so.1','etc/alpine-release']:
        member=members[name]
        assert member.isfile()
        path=selected/name
        path.parent.mkdir(exist_ok=True,parents=True)
        path.write_bytes(tar.extractfile(member).read())
        path.chmod(0o755 if name.startswith(("bin/", "lib/")) else 0o644)
proof='''#!/bin/sh
release=$(/bin/busybox cat /etc/alpine-release)
kernel=$(/bin/busybox uname -r)
pid1_exe=$(/bin/busybox readlink /proc/1/exe)
printf 'GLEWORKS_ALPINE_READY release=%s kernel=%s\\n' "$release" "$kernel"
while IFS= read -r challenge; do
    [ "${#challenge}" = 32 ] || continue
    case "$challenge" in *[!0-9a-f]*) continue;; esac
    [ "$release" = 3.24.3 ] || continue
    [ "$pid1_exe" = /bin/busybox ] || continue
    printf 'GLEWORKS_ALPINE_BOOT release=%s kernel=%s pid1_exe=%s challenge=%s\\n' "$release" "$kernel" "$pid1_exe" "$challenge"
done
'''
(selected/'etc/native-proof.sh').write_text(proof)
(selected/'etc/inittab').write_text('''::sysinit:/bin/busybox mount -t devtmpfs devtmpfs /dev
::sysinit:/bin/busybox mount -t proc proc /proc
::sysinit:/bin/busybox mount -t sysfs sysfs /sys
ttyGS0::respawn:/bin/busybox sh /etc/native-proof.sh
''')
subprocess.run(['sh','-n'],input=proof,text=True,check=True)
entries=['dir /'+name+' 0755 0 0' for name in ['bin','sbin','lib','etc','dev','proc','sys']]
entries+=['nod /dev/console 0600 0 0 c 5 1']
for name,mode in [('bin/busybox','0755'),('lib/ld-musl-armhf.so.1','0755'),('etc/alpine-release','0644'),('etc/native-proof.sh','0755'),('etc/inittab','0644')]:
    entries.append(f'file /{name} {selected/name} {mode} 0 0')
entries+=['slink /init /bin/busybox 0777 0 0','slink /bin/sh /bin/busybox 0777 0 0']
(base/'initramfs.list').write_text('\n'.join(entries)+'\n')
llvm='/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin/'
report={}
for name in ['bin/busybox','lib/ld-musl-armhf.so.1']:
    report[name]=subprocess.check_output([llvm+'llvm-readelf','-h','-l','-d','-A',str(selected/name)],text=True)
(base/'elf-inspection.txt').write_text('\n'.join(name+'\n'+value for name,value in report.items()))
assert '/lib/ld-musl-armhf.so.1' in report['bin/busybox']
assert 'libc.musl-armv7.so.1' in report['bin/busybox']
# Alpine's ELF SONAME differs from its executable loader path.
entries.append('slink /lib/libc.musl-armv7.so.1 /lib/ld-musl-armhf.so.1 0777 0 0')
(base/'initramfs.list').write_text('\n'.join(entries)+'\n')
print('Host-only Alpine BusyBox/musl/PID1/USB proof input prepared. No shell service, storage/network driver or device execution.')
