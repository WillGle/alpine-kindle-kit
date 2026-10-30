#!/usr/bin/env python3
"""Validate actual Alpine CPIO embedded in built kernel; no device installer."""
from pathlib import Path
import hashlib,json,re,struct,subprocess,zlib
base=Path(__file__).resolve().parent
kernel=(base/'build/arch/arm/boot/zImage').read_bytes()
compressed=(base/'build/arch/arm/boot/compressed/piggy_data').read_bytes()
raw=(base/'build/arch/arm/boot/Image').read_bytes()
cpio=(base/'build/usr/initramfs_data.cpio').read_bytes()
assert compressed in kernel and zlib.decompress(compressed,31)==raw
assert cpio in raw
files={};at=0
while cpio[at:at+6]==b'070701':
    fields=[int(cpio[at+6+i*8:at+14+i*8],16) for i in range(13)]
    length,namesize=fields[6],fields[11]
    name=cpio[at+110:at+110+namesize-1].decode()
    start=(at+110+namesize+3)&~3
    assert name not in files
    files[name]=(fields,cpio[start:start+length])
    at=(start+length+3)&~3
    if name=='TRAILER!!!':break
for name in ['bin/busybox','lib/ld-musl-armhf.so.1','etc/alpine-release','etc/inittab','etc/native-proof.sh']:
    assert files[name][1]==(base/'selected'/name).read_bytes(),name
    assert files[name][0][2:4]==[0,0]
assert files['init'][1]==b'/bin/busybox\0' and files['init'][0][1]==0o120777
assert files['bin/sh'][1]==b'/bin/busybox\0'
assert files['lib/libc.musl-armv7.so.1'][1]==b'/lib/ld-musl-armhf.so.1\0'
assert files['dev/console'][0][9:11]==[5,1]
assert files['bin/busybox'][0][1]==0o100755
assert struct.unpack_from('<I',kernel,36)[0]==0x016f2818
assert struct.unpack_from('<II',kernel,40)[1]-struct.unpack_from('<II',kernel,40)[0]==len(kernel)
assert len(kernel)%8==0
config=(base/'build/.config').read_text()
for option in ['MMC','MTD','NET','DRM','FB','PM','WATCHDOG','MODULES','ARM_ATAG_DTB_COMPAT']:
    assert '# CONFIG_'+option+' is not set\n' in config,option
assert 'CONFIG_GENERIC_IDLE_POLL_SETUP=y\n' in config
assert 'panic=0 nohlt' in config
assert 'CONFIG_LOCALVERSION="-gleworks-alpine-probe"' in config
for option in ['USB_G_SERIAL','VFP','BLK_DEV_INITRD','CMDLINE_FORCE']:
    assert 'CONFIG_'+option+'=y\n' in config
log=(base/'build.log').read_text()
assert 'error:' not in log
warnings=[line for line in log.splitlines() if 'warning:' in line]
assert all("argument unused during compilation: '-nostdlibinc'" in line for line in warnings)
dtb=(base.parent/'native-boot/imx6sl-pw3-probe.dtb').read_bytes()
assert struct.unpack_from('>II',dtb)==(0xd00dfeed,len(dtb))
image=kernel+dtb
(base/'alpine-zImage-dtb.bin').write_bytes(image)
review={'scope':'Host-built minimum Alpine BusyBox/musl RAM userspace, not a full Alpine installation',
 'kernel_release':(base/'build/include/generated/utsrelease.h').read_text().strip(),
 'kernel_bytes':len(kernel),'dtb_bytes':len(dtb),'combined_bytes':len(image),
 'combined_sha256':hashlib.sha256(image).hexdigest(),'inflated_image_bytes':len(raw),
 'cpio_bytes':len(cpio),'actual_embedded_CPIO_verified':True,
 'embedded_Alpine_release':files['etc/alpine-release'][1].decode().strip(),
 'init_symlink':'/bin/busybox','no_storage_network_PM_drivers':True,
 'build_warning_lines':warnings,
 'fits_existing_2MiB_stager':len(image)<=2*1024*1024,
 'device_booted':False,'boot_decision':'NO-GO: complete DMA/recovery and separately qualify larger fixed-image staging',
 'no_device_launcher_created':True}
(base/'host-review.json').write_text(json.dumps(review,indent=2)+'\n')
(base/'SHA256SUMS').write_text(''.join(hashlib.sha256((base/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in ['alpine-zImage-dtb.bin','build/.config','build/vmlinux','build/usr/initramfs_data.cpio']))
print(json.dumps(review,indent=2))
