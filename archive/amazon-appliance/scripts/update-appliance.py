#!/usr/bin/env python3
"""Update the complete controlled app layer in one pass; keep verified upstream binaries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
def sha(data): return hashlib.sha256(data).hexdigest()
def atomic_write(path,data):
    temporary=path.with_name(path.name+'.gleworks-writing')
    if temporary.exists() or temporary.is_symlink():
        raise ValueError('Interrupted temporary file needs review: '+str(temporary))
    try:
        with temporary.open('xb') as f:
            f.write(data);f.flush();os.fsync(f.fileno())
        temporary.replace(path)
    finally:
        if temporary.is_file(): temporary.unlink()

def ordinary_path(root,path):
    path.relative_to(root)
    for parent in (path,*path.parents):
        if parent==root: break
        if parent.is_symlink(): raise ValueError('Linked update path: '+str(parent))

def apply(mount,baseline):
    backup=mount/'gleworks-update-backup-v1'
    if backup.is_symlink(): raise ValueError('Unsafe backup directory')
    changes=[]
    for name in ('kindle-services.sh','show-message.sh','record-failure.sh','apply-bundle.sh','rollback.sh','gleworks-inspect.sh'):
        changes.append((ROOT/'scripts'/name,Path('gleworks/scripts')/name,baseline/'gleworks/scripts'/name))
    for name,baseline_name in (('gleworks-install.sh','gleworks-install.sh'),('gleworks-inspect.sh',None),('rollback.sh','gleworks-rollback.sh')):
        relative=Path('documents')/('gleworks-rollback.sh' if name=='rollback.sh' else name)
        # Inspection predates bundle packaging; admit only its known shipped source.
        original=baseline/baseline_name if baseline_name else ROOT/'baselines/gleworks-inspect.sh'
        changes.append((ROOT/'scripts'/name,relative,original))
    for directory in ('device','web','scriptlets'):
        for source in sorted((ROOT/directory).rglob('*')):
            if not source.is_file(): continue
            relative=Path('gleworks')/source.relative_to(ROOT)
            changes.append((source,relative,baseline/relative))
            if directory=='scriptlets':
                changes.append((source,Path('documents')/source.name,baseline/'gleworks/scriptlets'/source.name))
    for source in sorted((ROOT/'plugins').rglob('*.lua')):
        relative=Path('koreader/plugins')/source.relative_to(ROOT/'plugins')
        changes.append((source,relative,baseline/relative))
    # Accept the recorded previous own revision; unexpected unrecorded edits stop.
    previous={}
    for name,kind in (('patch-reader-vpn.json','reader-vpn-feedback-v1'),('appliance-release.json','appliance-v1')):
        path=mount/'gleworks'/name
        if path.is_symlink(): raise ValueError('Unsafe installed revision record')
        if path.is_file():
            record=json.loads(path.read_text())
            if record.get('patch')!=kind: raise ValueError('Unrecognized installed revision')
            for relative,value in record.get('files',{}).items(): previous.setdefault(relative,set()).add(value)
            for relative,value in record.get('backup_files',{}).items(): previous.setdefault(relative,set()).add(value)
    payload=[]
    # Preflight every destination before changing any installed code.
    for source,relative,original in changes:
        path=mount/relative;data=source.read_bytes()
        ordinary_path(mount,path)
        ordinary_path(mount,backup/relative)
        if path.is_symlink() or (path.exists() and not path.is_file()): raise ValueError('Unsafe patch path: '+str(relative))
        before=path.read_bytes() if path.exists() else None
        expected=original.read_bytes() if original.is_file() else None
        if before not in (data,expected) and (before is None or sha(before) not in previous.get(str(relative),set())):
            raise ValueError('Installed file changed; review before updating: '+str(relative))
        saved=backup/relative
        if saved.is_symlink() or (saved.exists() and (not saved.is_file() or saved.read_bytes()!=before and sha(saved.read_bytes()) not in previous.get(str(relative),set()) and saved.read_bytes()!=expected)):
            raise ValueError('Conflicting patch backup: '+str(relative))
        payload.append((relative,data,before))
    settings=mount/'koreader/settings.reader.lua'
    if settings.is_symlink() or not settings.is_file(): raise ValueError('Missing ordinary reader settings')
    ordinary_path(mount,settings)
    raw=settings.read_bytes();updated=raw
    for key in ('full_refresh_count','night_full_refresh_count'):
        pattern=rb'(\["'+key.encode()+rb'"\]\s*=\s*)\d+(\s*,)'
        updated,n=re.subn(pattern,lambda match:match[1]+b'4'+match[2],updated)
        if n!=1: raise ValueError('Expected one numeric reader setting: '+key)
    settings_backup=settings.with_name(settings.name+'.gleworks-before-refresh4')
    if settings_backup.is_symlink(): raise ValueError('Unsafe settings backup')
    if updated!=raw and settings_backup.exists(): raise ValueError('Refresh backup already exists; review preferences before changing them')
    required=sum(len(data)+(len(before) if before else 0) for _,data,before in payload)+len(raw)*2+16*1024*1024
    if shutil.disk_usage(mount).free<required: raise ValueError('Insufficient update/backup space')
    backup.mkdir(exist_ok=True)
    for relative,data,before in payload:
        if before==data: continue
        if before is not None:
            saved=backup/relative;saved.parent.mkdir(parents=True,exist_ok=True)
            if not saved.exists(): atomic_write(saved,before)
        (mount/relative).parent.mkdir(parents=True,exist_ok=True)
        atomic_write(mount/relative,data)
    if updated!=raw:
        atomic_write(settings_backup,raw)
        atomic_write(settings,updated)
    record={'patch':'appliance-v1','full_refresh_count':4,
            'files':{str(relative):sha(data) for relative,data,_ in payload},
            'backup':backup.name,'backup_files':{str(relative):sha((backup/relative).read_bytes()) for relative,_,_ in payload if (backup/relative).is_file()},'runtime_acceptance':'pending'}
    atomic_write(mount/'gleworks/appliance-release.json',(json.dumps(record,indent=2)+'\n').encode())
    for relative,data,_ in payload:
        if (mount/relative).read_bytes()!=data: raise ValueError('Patch readback mismatch: '+str(relative))
    if settings.read_bytes()!=updated: raise ValueError('Settings readback mismatch')
    os.sync()
    print('Complete app layer readback-verified: Reader, VPN, Transfer, Terminal, Calendar/Todos, Update Apps and Status.')
    print('Safely eject USB. Check Reader, Calendar/Todos, Terminal, Update Apps, VPN/Login and Transfer, then open Status once.')
    print('Reconnect USB afterward: documents/GleWorks-status.txt contains the shared diagnosis report.')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mount',type=Path,default=Path('/run/media/will/Kindle'))
    args=parser.parse_args();mount=args.mount.resolve(strict=True)
    entries=[line.split() for line in Path('/proc/mounts').read_text().splitlines()]
    entry=next((m for m in entries if m[1].replace('\\040',' ')==str(mount)),None)
    if not entry or entry[2]!='vfat' or 'rw' not in entry[3].split(','):
        raise ValueError('Expected the connected read-write Kindle VFAT mount')
    if not (mount/'gleworks/UPSTREAM.json').is_file(): raise ValueError('Installed GleWorks not found')
    apply(mount,ROOT/'bundle')

if __name__=='__main__':
    try: main()
    except Exception as exc: raise SystemExit('Update stopped: '+str(exc))
