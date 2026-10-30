#!/usr/bin/env python3
"""Observe global Alpine PID1/root/guardian through a fresh physical USB challenge."""
from pathlib import Path
import argparse
import json
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'native-display'))
from usb_console import Console
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
console=Console(seconds=180)
try:
    result=console.run('''set -eu
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(uname -r)" = 3.0.35-lab126 ]
cat /etc/alpine-release
cat /proc/sys/kernel/random/boot_id
cat /proc/mounts
/gleworks/guard-control status
''')
    assert '/ ext3 ro,' in result and 'state=00000002' in result,result
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps({'scope':'Fresh physical USB nonce, global Alpine PID1, readonly root and armed guardian; stock return separately required','verified':True,'output':result},indent=2)+'\n')
    print('Readonly trial observed. Existing guardian remains armed; verify stock return separately.')
finally:console.close()
