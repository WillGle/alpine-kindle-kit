#!/usr/bin/env python3
"""Acquire pinned official ARMv7 userspace; never extract or run target code."""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import ssl
import tarfile
import urllib.request

BASE = Path(__file__).resolve().parent
NAME = 'alpine-minirootfs-3.24.3-armv7.tar.gz'
URL = 'https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/armv7/'
SHA256 = 'dc1ddfaf03ede4be79ad7475490f6b7859dbcbbbbb85150aa1ee013b4bc2f0c1'
SHA512 = 'a2e278d280483706a358d0f5ae9622d1f44ed374125d6dc9949b555fdfe8cecb10fcea815178b3c9a26719e1922adb78735293a4aacdb3e58f56f4d395bc9d30'
context = ssl.create_default_context(cafile='/etc/ssl/certs/ca-certificates.crt')
def fetch(name, limit):
    with urllib.request.urlopen(URL+name, context=context, timeout=30) as response:
        data = response.read(limit+1)
    if len(data)>limit:
        raise ValueError('download exceeds bound: '+name)
    return data
path = BASE/NAME
if path.exists():
    data=path.read_bytes()
else:
    data=fetch(NAME, 4*1024*1024)
assert len(data)==3183009
assert hashlib.sha256(data).hexdigest()==SHA256
assert hashlib.sha512(data).hexdigest()==SHA512
published=fetch(NAME+'.sha256', 4096)
assert published.decode().split()[0]==SHA256
published512=fetch(NAME+'.sha512', 4096)
assert published512.decode().split()[0]==SHA512
with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as archive:
    members=archive.getmembers()
    for item in members:
        parts=PurePosixPath(item.name).parts
        assert not item.name.startswith('/') and '..' not in parts,item.name
    files={m.name.removeprefix('./'):m for m in members}
    release=archive.extractfile(files['etc/alpine-release']).read().decode().strip()
    assert release=='3.24.3'
    selected={}
    for name in ['bin/busybox','lib/ld-musl-armhf.so.1','sbin/apk']:
        item=files[name]
        assert item.isfile(),name
        payload=archive.extractfile(item).read()
        assert payload[:6]==b'\x7fELF\x01\x01' and int.from_bytes(payload[18:20],'little')==40,name
        selected[name]={'bytes':len(payload),'sha256':hashlib.sha256(payload).hexdigest()}
if not path.exists():path.write_bytes(data)
(BASE/(NAME+'.sha256')).write_bytes(published)
(BASE/(NAME+'.sha512')).write_bytes(published512)
review={'source':URL+NAME,'release':release,'arch':'armv7','archive_bytes':len(data),
        'sha256':SHA256,'sha512':SHA512,'checksums_match_official_https_files':True,
        'publisher_signature_verified':False,'archive_members':len(members),
        'regular_file_bytes':sum(m.size for m in members if m.isfile()),
        'selected_ARM32_ELF':selected,'archive_extracted':False,
        'target_code_executed':False,'bootable_image':False,'device_booted':False}
(BASE/'rootfs-review.json').write_text(json.dumps(review,indent=2)+'\n')
print(json.dumps(review,indent=2))
