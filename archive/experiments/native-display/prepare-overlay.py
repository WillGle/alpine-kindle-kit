#!/usr/bin/env python3
"""Collect exact reader, generic compatibility libraries and captured PW3 touch modules."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import tarfile

base=Path(__file__).resolve().parent
linux=base.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reader',type=Path,default=linux.parent/'bundle/koreader')
parser.add_argument('--p1-backup',type=Path,default=linux.parent/'docs/evidence/private/pretrial-backup/p1.ext3')
parser.add_argument('--modules',type=Path,default=linux.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/stock-modules.tar')
parser.add_argument('--qemu-arm',default=shutil.which('qemu-arm'))
args=parser.parse_args()
overlay=base/'overlay'
compat=overlay/'opt/reader-compat'
compat.mkdir(parents=True,exist_ok=True)
libs={'ld-linux.so.3':'ld-2.20.so','libc.so.6':'libc-2.20.so','libm.so.6':'libm-2.20.so','libdl.so.2':'libdl-2.20.so','libpthread.so.0':'libpthread-2.20.so','librt.so.1':'librt-2.20.so','libgcc_s.so.1':'libgcc_s.so.1','libresolv.so.2':'libresolv-2.20.so','libnss_files.so.2':'libnss_files-2.20.so','libnss_dns.so.2':'libnss_dns-2.20.so'}
for name,source in libs.items():
    target=compat/name
    subprocess.run(['debugfs','-R',f'dump /lib/{source} {target}',str(args.p1_backup)],capture_output=True,text=True,check=True)
    assert target.is_file() and target.stat().st_size>1000,name
    target.chmod(0o755)
reader=overlay/'opt/koreader'
shutil.copytree(args.reader,reader,symlinks=True,dirs_exist_ok=True)
# This optional module wraps Amazon IPC; native mode does not use that API.
(reader/'common/libopenlipclua.so').unlink(missing_ok=True)
shutil.copyfile(base/'native.lua',reader/'native.lua')
with tarfile.open(args.modules) as tar:
    for name in ['cyttsp4_i2c','cyttsp4_core','cyttsp4_mt_b']:
        rel=f'lib/modules/3.0.35-lab126/kernel/drivers/input/touchscreen/{name}.ko'
        data=tar.extractfile(rel).read()
        assert b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in data
        target=overlay/rel
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
        target.chmod(0o644)
for name in ['gleworks-display.sh','gleworks-reader-service.sh']:
    target=overlay/'etc'/name
    target.parent.mkdir(exist_ok=True)
    shutil.copyfile(base/name,target)
    target.chmod(0o755)
    subprocess.run(['sh','-n',str(target)],check=True)
qemu=args.qemu_arm
if not qemu:
    choices=list(Path('/nix/store').glob('*qemu-user*/bin/qemu-arm'))
    assert choices,'Install qemu-user or pass --qemu-arm'
    qemu=str(choices[0])
loader=compat/'ld-linux.so.3'
paths=str(compat)+':'+str(reader/'libs')
for name,arguments in [('luajit',['-v']),('fbink',['-h']),('luajit',['-e',f'assert(loadfile("{base}/native.lua")); print("native wrapper syntax verified")'])]:
    result=subprocess.run([qemu,str(loader),'--library-path',paths,str(reader/name),*arguments],capture_output=True,text=True,timeout=20)
    assert result.returncode==0,result.stdout+result.stderr
manifest={'reader_revision':(reader/'git-rev').read_text().strip(),'p1_backup_sha256':hashlib.sha256(args.p1_backup.read_bytes()).hexdigest(),'module_archive_sha256':hashlib.sha256(args.modules.read_bytes()).hexdigest(),'amazon_services_included':False,'native_lua_sha256':hashlib.sha256((reader/'native.lua').read_bytes()).hexdigest()}
(base/'overlay-inputs.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Reader overlay and ARM binary checks complete; no device operation.')
