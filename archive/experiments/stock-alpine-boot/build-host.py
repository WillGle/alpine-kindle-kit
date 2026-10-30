#!/usr/bin/env python3
"""Build a private source-supported MULTI candidate; no installer or device I/O."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import tarfile
import zlib

base = Path(__file__).resolve().parent
root = base.parent
private = base / '.private'
private.mkdir(mode=0o700, exist_ok=True)
private.chmod(0o700)
stock = (root.parent / 'docs/evidence/private/captures/boot-preparation/stock-main.uImage').read_bytes()
assert hashlib.sha256(stock).hexdigest() == 'd99091ac41ab95749a85b91ddfa4f15a1b05750fe4534321ab6a0dc404200628'
fmt = '>7I4B32s'
fields = list(struct.unpack_from(fmt, stock))
assert fields[0] == 0x27051956 and fields[3] + 64 == len(stock)
header = bytearray(stock[:64]); header[4:8] = b'\0' * 4
assert zlib.crc32(header) == fields[1]
kernel = stock[64:]
assert zlib.crc32(kernel) == fields[6]
assert fields[4:6] == [0x80008000, 0x80008000]
assert fields[7:11] == [5, 2, 2, 0]  # Linux, ARM, kernel, no outer compression

inputs = {
    'bin/busybox': root / 'alpine-boot/selected/bin/busybox',
    'lib/ld-musl-armhf.so.1': root / 'alpine-boot/selected/lib/ld-musl-armhf.so.1',
    'etc/alpine-release': root / 'alpine-boot/selected/etc/alpine-release',
}
assert hashlib.sha256(inputs['bin/busybox'].read_bytes()).hexdigest() == 'df4dd71f89214032872e60328f3af03faf35e344f6fa5493656d8775d35e930c'
assert hashlib.sha256(inputs['lib/ld-musl-armhf.so.1'].read_bytes()).hexdigest() == '2acf3c82bc97b496c40e1d5007d4258570c8df0edc45793d6651a352fb778990'
modules = ['kernel/drivers/usb/otg/fsl_otg_arc.ko',
           'kernel/drivers/usb/gadget/arcotg_udc.ko',
           'kernel/drivers/usb/gadget/g_serial.ko']
module_hashes = {}
with tarfile.open(root.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/stock-modules.tar') as tar:
    for name in modules:
        data = tar.extractfile('lib/modules/3.0.35-lab126/' + name).read()
        assert data[:6] == b'\x7fELF\x01\x01'
        assert b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in data
        path = private / Path(name).name
        path.write_bytes(data); path.chmod(0o600)
        inputs['modules/' + path.name] = path
        module_hashes[path.name] = hashlib.sha256(data).hexdigest()

setup = '''#!/bin/sh
set -eu
bb=/bin/busybox
$bb mount -t proc proc /proc
$bb mount -t sysfs sysfs /sys
# Stock kernel has no devtmpfs; only exact USB modules and RAM device nodes.
$bb insmod /modules/fsl_otg_arc.ko
$bb insmod /modules/arcotg_udc.ko
$bb insmod /modules/g_serial.ko use_acm=1 n_ports=1
major=$($bb awk '$2 == "ttyGS" {print $1}' /proc/devices)
case "$major" in ''|*[!0-9]*) exit 1;; esac
$bb mknod /dev/ttyGS0 c "$major" 0
$bb chmod 600 /dev/ttyGS0
'''
proof = '''#!/bin/sh
bb=/bin/busybox
release=$($bb cat /etc/alpine-release)
kernel=$($bb uname -r)
pid1=$($bb readlink /proc/1/exe)
while IFS= read -r challenge; do
    [ "${#challenge}" = 32 ] || continue
    case "$challenge" in *[!0-9a-f]*) continue;; esac
    [ "$release" = 3.24.3 ] && [ "$kernel" = 3.0.35-lab126 ] && [ "$pid1" = /bin/busybox ] || continue
    printf 'GLEWORKS_STOCK_ALPINE_BOOT release=%s kernel=%s pid1_exe=%s challenge=%s\\n' "$release" "$kernel" "$pid1" "$challenge"
done
'''
inittab = '''::sysinit:/bin/busybox sh /etc/setup-usb.sh
ttyGS0::respawn:/bin/busybox sh /etc/proof.sh
'''
for name, value in [('setup-usb.sh', setup), ('proof.sh', proof), ('inittab', inittab)]:
    path = private / name
    path.write_text(value); path.chmod(0o600)
    inputs['etc/' + name] = path
    if name.endswith('.sh'):
        subprocess.run(['sh', '-n'], input=value, text=True, check=True)
entries = ['dir /' + name + ' 0755 0 0' for name in ['bin', 'sbin', 'lib', 'etc', 'dev', 'proc', 'sys', 'modules', 'tmp']]
entries += ['nod /dev/console 0600 0 0 c 5 1', 'nod /dev/null 0666 0 0 c 1 3']
for name, path in inputs.items():
    mode = '0755' if name in ['bin/busybox', 'lib/ld-musl-armhf.so.1'] or name.endswith('.sh') else '0644'
    entries.append(f'file /{name} {path} {mode} 0 0')
entries += ['slink /init /bin/busybox 0777 0 0', 'slink /bin/sh /bin/busybox 0777 0 0',
            'slink /lib/libc.musl-armv7.so.1 /lib/ld-musl-armhf.so.1 0777 0 0']
manifest = private / 'initramfs.list'
manifest.write_text('\n'.join(entries) + '\n'); manifest.chmod(0o600)
cpio = subprocess.check_output([str(root / 'alpine-boot/build/usr/gen_init_cpio'), '-t', '0', str(manifest)])
assert cpio.startswith(b'070701') and b'TRAILER!!!\0' in cpio
(private / 'alpine-initramfs.cpio').write_bytes(cpio)
payload = struct.pack('>III', len(kernel), len(cpio), 0) + kernel
payload += b'\0' * (-len(kernel) % 4) + cpio
fields[1] = 0; fields[3] = len(payload); fields[6] = zlib.crc32(payload)
fields[9] = 4  # IH_TYPE_MULTI: kernel first, raw initramfs second
fields[11] = b'GleWorks stock Alpine HOST ONLY\0'
fields[1] = zlib.crc32(struct.pack(fmt, *fields))
candidate = struct.pack(fmt, *fields) + payload
assert len(candidate) < 14 * 1024 * 1024
out = private / 'stock-alpine-HOST-ONLY.uImage'
out.write_bytes(candidate); out.chmod(0o600)
rd_start = 0x80800000 + 64 + 12 + ((len(kernel) + 3) & ~3)
# Actual captured head instructions corroborate this source-derived overlap
# path. This proves arithmetic separation, not reserved-memory or CPU behavior.
assert struct.unpack_from('<I', kernel, 0xbc)[0] == 0xe28aab02  # add r10,2048
assert struct.unpack_from('<I', kernel, 0xc4)[0] == 0xe24f5064  # adr r5,restart
lc0 = struct.unpack_from('<8I', kernel, 0x17c)
assert lc0[0] == 0x17c and lc0[3] == len(kernel)
inflated = struct.unpack_from('<I', kernel, lc0[4])[0]
assert inflated == 6994012
entry = 0x80008000
copy_start = (entry + inflated + 2048) & ~255
source = entry + 0x60  # restart0x68 rounded down32
delta = copy_start - source
copy_end = copy_start + ((entry + len(kernel) - source + 31) & ~31)
stack_top = entry + lc0[7] + delta
heap_end = stack_top + 65536
assert copy_end < stack_top < heap_end < 0x80800000 + len(candidate)
assert heap_end < rd_start
report = {'scope': 'Host-only legacy MULTI image hypothesis; no install or boot command',
          'exact_stock_kernel_preserved': True, 'kernel_payload_bytes': len(kernel),
          'uncompressed_cpio_bytes': len(cpio), 'candidate_bytes': len(candidate),
          'candidate_sha256': hashlib.sha256(candidate).hexdigest(),
          'cpio_sha256': hashlib.sha256(cpio).hexdigest(), 'stock_module_sha256': module_hashes,
          'source_config_loadaddr': '0x80800000', 'source_initrd_start': hex(rd_start),
          'source_initrd_end_exclusive': hex(rd_start + len(cpio)),
          'source_initrd_ATAG_supported': True, 'stock_kernel_INITRD_supported': True,
          'stock_kernel_has_devtmpfs': False, 'kernel_module_ABI_rebuilt': False,
          'captured_decompressor_envelope': {
              'inflated_bytes': inflated, 'copied_code_start': hex(copy_start),
              'copied_code_end_exclusive': hex(copy_end), 'stack_top': hex(stack_top),
              'heap_end_exclusive': hex(heap_end), 'initrd_not_overlapped_by_derived_envelope': True,
              'hardware_reserved_memory_qualified': False},
          'production_bootloader_MULTI_execution_verified': False,
          'userspace_ABI_on_Kindle_verified': False, 'early_USB_module_order_on_Kindle_verified': False,
          'cold_boot_decompressor_memory_verified': False, 'cable_only_recovery_verified': False,
          'device_booted': False, 'flash_decision': 'NO-GO'}
(base / 'host-review.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
