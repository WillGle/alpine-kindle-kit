#!/usr/bin/env python3
"""Repeat the original stock inspection/backup through an already-pinned temporary SSH session."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import secrets
import argparse
import hashlib
import json
import os
import subprocess

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from pw3_common import TOOLS, add_work_dir, workspace
base=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['system','boot','modules','backup-p1'])
parser.add_argument('--ssh-config',type=Path,required=True)
parser.add_argument('--output',type=Path)
add_work_dir(parser)
args=parser.parse_args()
args.output=args.output or workspace(args.work_dir)/'private/captures'/(args.action+'-'+secrets.token_hex(6))
config=json.loads(args.ssh_config.read_text())
ssh=config['ssh_argv']
assert ssh[0]=='ssh' and 'StrictHostKeyChecking=yes' in ssh and 'BatchMode=yes' in ssh
assert args.output.name not in {'.','..'} and not args.output.exists()
args.output.mkdir(parents=True,mode=0o700)

def run(script):
    result=subprocess.run(ssh+['sh -s'],input=script,text=True,capture_output=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    return result.stdout

baseline='''set -eu
[ "$(readlink /proc/1/exe)" = /sbin/init.exe ]
[ "$(uname -r)" = 3.0.35-lab126 ]
cat /proc/sys/kernel/random/boot_id
readlink /proc/1/exe
grep '^/dev/root / .* ext3 ro,' /proc/mounts
cat /sys/class/block/mmcblk0p1/size
'''
before=run(baseline)
if args.action=='backup-p1':
    assert before.strip().endswith('921600'),before
    path=args.output/'p1.ext3.partial'
    with path.open('xb') as stream:
        path.chmod(0o600)
        result=subprocess.run(ssh+['dd if=/dev/mmcblk0p1 bs=1048576'],stdout=stream,stderr=subprocess.PIPE,timeout=300)
        stream.flush()
        os.fsync(stream.fileno())
    assert result.returncode==0,result.stderr
    assert path.stat().st_size==921600*512
    after=run(baseline)
    assert after==before,'Device rebooted or mount state changed while capturing'
    final=args.output/'p1.ext3'
    path.rename(final)
    h=hashlib.sha256()
    with final.open('rb') as stream:
        while data:=stream.read(1024*1024):h.update(data)
    record={'scope':'Exact original readonly p1; capture before any p1 write','bytes':final.stat().st_size,'sha256':h.hexdigest(),'before':before,'after':after}
    (args.output/'review.json').write_text(json.dumps(record,indent=2)+'\n')
else:
    if args.action=='system':
        source=base/'capture/gleworks-linux-probe.sh'
    else:
        source=base/('capture/gleworks-'+('boot' if args.action=='boot' else 'module')+'-capture.sh')
    text=run(source.read_text())
    (args.output/'capture.log').write_text(text)
    (args.output/'baseline.log').write_text(before)
print('Captured',args.action,'into private',args.output)
