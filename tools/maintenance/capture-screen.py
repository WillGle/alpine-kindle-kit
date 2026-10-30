#!/usr/bin/env python3
"""Capture the visible PW3 8bpp framebuffer over its physical USB console."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse
import secrets
import base64
import gzip
import re
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from usb_console import Console
from pw3_common import add_work_dir, workspace

parser=argparse.ArgumentParser(description=__doc__)
add_work_dir(parser)
parser.add_argument('--output', type=Path)
args=parser.parse_args()
output=args.output or workspace(args.work_dir)/'private/screens'/('screen-'+secrets.token_hex(6)+'.png')
assert not output.exists(), 'Preserve existing screenshots'
output.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
console=Console()
try:
    text=console.run("printf '%s%s\\n' SCREEN_ BEGIN; /bin/busybox dd if=/dev/fb0 bs=1088 count=1448 2>/dev/null | /bin/busybox gzip -c | /bin/busybox base64; printf '%s%s\\n' SCREEN_ END",seconds=60)
finally:
    console.close()
match=re.search(r'\r?\nSCREEN_BEGIN\r?\n(.*?)\r?\nSCREEN_END',text,re.S)
assert match,text[:1000]
raw=gzip.decompress(base64.b64decode(match[1]))
assert len(raw)==1088*1448,len(raw)
import struct
import zlib
def chunk(name,data):
    return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
pixels=b''.join(b'\0'+raw[row*1088:row*1088+1072] for row in range(1448))
header=struct.pack('>IIBBBBB',1072,1448,8,0,0,0,0)
with output.open('xb') as stream:
    stream.write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',header)+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b''))
output.chmod(0o600)
print('Captured framebuffer:',output)
