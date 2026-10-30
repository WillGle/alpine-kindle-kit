#!/usr/bin/env python3
"""Verify every source/image member and optionally extract a package into a new directory."""
from pathlib import Path, PurePosixPath
import sys
sys.dont_write_bytecode = True
import argparse
import hashlib
import json
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('archive',type=Path)
parser.add_argument('--extract-to',type=Path)
args=parser.parse_args()
with tarfile.open(args.archive,'r:gz') as archive:
    members=archive.getmembers()
    assert len(members)<10000
    names=set()
    for member in members:
        rel=PurePosixPath(member.name)
        assert not rel.is_absolute() and '..' not in rel.parts and rel.parts[0]=='kindle-native'
        assert member.isfile() and member.name not in names,member.name
        names.add(member.name)
    manifest=json.load(archive.extractfile('kindle-native/SOURCE-SHA256.json'))
    expected={'kindle-native/'+name for name in manifest}|{'kindle-native/SOURCE-SHA256.json'}
    assert names==expected,'Archive/manifest membership differs'
    for name,wanted in manifest.items():
        h=hashlib.sha256()
        stream=archive.extractfile('kindle-native/'+name)
        while data:=stream.read(1024*1024):h.update(data)
        assert h.hexdigest()==wanted,name
    if args.extract_to is not None:
        assert not args.extract_to.exists(),'Extraction target must be new'
        args.extract_to.mkdir(parents=True,mode=0o700)
        # Members are validated regular files; never restore owners or special bits.
        for member in members:
            target=args.extract_to/member.name
            target.parent.mkdir(parents=True,exist_ok=True)
            with target.open('xb') as output:
                source=archive.extractfile(member)
                while data:=source.read(1024*1024):output.write(data)
            target.chmod(0o755 if member.mode&0o111 else 0o644)
print('Verified exact manifest and SHA256 for',len(manifest),'files.')
