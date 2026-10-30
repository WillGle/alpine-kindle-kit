from pathlib import Path
import subprocess,secrets,tarfile,hashlib,json
linux=Path('/home/will/dev/kindle/ours/linux');p=linux/'native-rootfs-v4-diagnostic';p.mkdir(exist_ok=False)
image=p/'alpine-diagnostic.ext3';subprocess.run(['cp','--reflink=auto',str(linux/'native-rootfs-v3-trial/.private/alpine-native-root-HOST-ONLY.ext3'),str(image)],check=True)
def read(name):
 r=subprocess.run(['debugfs','-R','cat '+name,str(image)],text=True,capture_output=True,check=True);assert 'not found' not in r.stderr;return r.stdout
nonce=secrets.token_hex(16)
early=read('/etc/gleworks-early.sh')
prefix='''#!/bin/sh
set -eu
exec >> /boot/gleworks-control/gleworks-alpine/coldboot.log 2>&1
echo GW_PHYSICAL_COLDSTART_NONCE=NONCE
/bin/busybox sync
snapshot() {
 bb=/bin/busybox
 echo GW_ALPINE_SNAPSHOT
 $bb cat /proc/uptime /proc/sys/kernel/random/boot_id /etc/alpine-release
 $bb readlink /proc/1/exe /proc/1/root
 /gleworks/guard-control status
 $bb cat /proc/modules /proc/mounts
 $bb cat /proc/asession /proc/driver/fsl_otg
 $bb cat /sys/devices/platform/fsl-usb2-udc/gadget/connected
 $bb ps
}
trap 'rc=$?; set +e; echo GW_SYSINIT_RESULT=$rc; snapshot; /bin/busybox sync' EXIT
'''.replace('=NONCE','='+nonce)
early=prefix+early.split('\n',1)[1]+'''snapshot
/bin/busybox sync
/bin/busybox sh /etc/gleworks-usb-retry.sh >> /boot/gleworks-control/gleworks-alpine/coldboot.log 2>&1 &
'''
retry='''#!/bin/sh
bb=/bin/busybox
connected=/sys/devices/platform/fsl-usb2-udc/gadget/connected
$bb sleep 10
if [ "$($bb cat "$connected")" != 1 ]; then
 echo GW_USB_RETRY_VENDOR_BSESSION
 $bb cat /proc/asession /proc/driver/fsl_otg
 # Same vendor B-session helper as its boot-cable detection path; no host mode.
 printf '5 0\\n' > /proc/asession
 $bb sleep 1
 printf '5 1\\n' > /proc/asession
 $bb sleep 10
 $bb cat "$connected" /proc/asession /proc/driver/fsl_otg
 $bb sync
fi
if [ "$($bb cat "$connected")" != 1 ]; then
 echo GW_USB_RETRY_STOCK_EHCI_DEPENDENCY
 $bb insmod /gleworks/ehci-hcd.ko
 $bb sleep 10
 $bb cat "$connected" /proc/asession /proc/driver/fsl_otg
 $bb sync
fi
echo GW_USB_RETRY_COMPLETE
$bb sync
'''
proof=read('/etc/gleworks-proof.sh').replace('\\\\n','\\n')
inputs={'/etc/gleworks-early.sh':early.encode(),'/etc/gleworks-usb-retry.sh':retry.encode(),'/etc/gleworks-proof.sh':proof.encode()}
with tarfile.open(linux.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/stock-modules.tar') as tar:
 m=next(m for m in tar.getmembers() if m.name.endswith('/ehci-hcd.ko'));inputs['/gleworks/ehci-hcd.ko']=tar.extractfile(m).read()
commands=''
for dest,data in inputs.items():
 src=p/dest.rsplit('/',1)[1];src.write_bytes(data)
 if dest.endswith('.sh'):subprocess.run(['sh','-n'],input=data,check=True)
 if dest!='/etc/gleworks-usb-retry.sh' and dest!='/gleworks/ehci-hcd.ko':commands+='rm '+dest+'\n'
 commands+=f'write {src} {dest}\nset_inode_field {dest} uid 0\nset_inode_field {dest} gid 0\nset_inode_field {dest} mode 0100'+('755' if dest.endswith('.sh') else '600')+'\n'
r=subprocess.run(['debugfs','-w',str(image)],input=commands,text=True,capture_output=True,check=True);(p/'prepare.log').write_text(r.stdout+r.stderr);assert 'not found' not in r.stderr
r=subprocess.run(['e2fsck','-fn',str(image)],text=True,capture_output=True);(p/'filesystem-check.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
review={'scope':'Physical coldboot diagnostic root; readonly root with bounded journal on mounted FAT','nonce':nonce,'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'base':'native-rootfs-v3-trial','usb_methods':['unchanged baseline10s','vendor /proc/asession B-session5 0/5 1','exact captured stock ehci-hcd if still disconnected'],'input_sha256':{n:hashlib.sha256(d).hexdigest() for n,d in inputs.items()},'reset_seconds':90}
(p/'review.json').write_text(json.dumps(review,indent=2)+'\n');print(json.dumps(review,indent=2))
