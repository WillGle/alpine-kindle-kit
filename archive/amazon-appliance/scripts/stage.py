#!/usr/bin/env python3
"""Copy a complete verified bundle to the connected Kindle; never eject or execute it."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[1]
def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()

def copy_payload(bundle, target, entries):
    files={name for _,name in entries} | {'SHA256SUMS','READY'}
    directories={parent.as_posix() for name in files for parent in Path(name).parents if parent!=Path('.')}
    if target.is_symlink() or (target.exists() and not target.is_dir()):
        raise ValueError('Unsafe staging path')
    if target.exists():
        # An interrupted copy may contain partial known files, never unrelated data.
        for p in target.rglob('*'):
            name=p.relative_to(target).as_posix()
            if p.is_symlink() or (p.is_dir() and name not in directories) or (not p.is_dir() and name not in files):
                raise ValueError('Unexpected staging member; review before retrying: '+name)
    complete=(target/'READY').exists()
    if complete:
        for expected,name in entries:
            if not (target/name).is_file() or digest(target/name)!=expected:
                raise ValueError('Completed staging differs from bundle; review before replacing it')
        if (target/'READY').read_bytes()!=(bundle/'READY').read_bytes() or (target/'SHA256SUMS').read_bytes()!=(bundle/'SHA256SUMS').read_bytes():
            raise ValueError('Completed staging metadata differs from bundle')
        return
    target.mkdir(exist_ok=True)
    for expected,name in entries:
        destination=target/name
        if destination.is_file() and digest(destination)==expected:
            continue
        if destination.exists() and not destination.is_file():
            raise ValueError('Staging file path is occupied: '+name)
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(bundle/name,destination)
    shutil.copyfile(bundle/'SHA256SUMS',target/'SHA256SUMS')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--update',action='store_true',help='Stage one complete app update, without reinstalling the jailbreak or migration')
    parser.add_argument('--bundle',type=Path,default=ROOT/'bundle')
    parser.add_argument('--mount',type=Path,default=Path('/run/media/will/Kindle'))
    args=parser.parse_args()
    bundle=args.bundle.resolve(strict=True); mount=args.mount.resolve(strict=True)
    if not (bundle/'READY').is_file(): raise ValueError('Incomplete bundle')
    # Verify the actual removable mount, not just a same-named local directory.
    mounts=[line.split() for line in Path('/proc/mounts').read_text().splitlines()]
    entry=next((m for m in mounts if m[1].replace('\\040',' ')==str(mount)),None)
    if not entry or entry[2]!='vfat' or 'rw' not in entry[3].split(','):
        raise ValueError('Expected a read-write Kindle VFAT mount')
    if not (mount/'system/version.txt').is_file() or not (mount/'kmc/kpm/kpm.db').is_file():
        raise ValueError('Expected the inspected Kindle layout')
    entries=[line.split('  ',1) for line in (bundle/'SHA256SUMS').read_text().splitlines()]
    for expected,name in entries:
        p=bundle/name
        if Path(name).is_absolute() or '..' in Path(name).parts or not p.resolve(strict=True).is_relative_to(bundle):
            raise ValueError('Unsafe manifest path')
        if digest(p)!=expected: raise ValueError('Bundle checksum mismatch: '+name)
    target=mount/('gleworks-updates' if args.update else 'gleworks-stage')
    if args.update and not (mount/'gleworks/scripts/apply-bundle.sh').is_file():
        raise ValueError('Install the complete app-layer revision before staging updates')
    for name in (() if args.update else ('gleworks-install.sh','gleworks-rollback.sh')):
        entrypoint=mount/'documents'/name
        if entrypoint.is_symlink() or (entrypoint.exists() and (not entrypoint.is_file() or digest(entrypoint)!=digest(bundle/name))):
            raise ValueError('Different entrypoint already exists; review before retrying')
    size=sum(p.stat().st_size for p in bundle.rglob('*') if p.is_file())
    if shutil.disk_usage(mount).free < size+64*1024*1024: raise ValueError('Insufficient staging space')
    # Resume only known files. READY is written after complete readback and entrypoints.
    copy_payload(bundle,target,entries)
    for expected,name in entries:
        if digest(target/name)!=expected: raise ValueError('Device readback mismatch: '+name)
    for name in (() if args.update else ('gleworks-install.sh','gleworks-rollback.sh')):
        shutil.copyfile(target/name,mount/'documents'/name)
    shutil.copyfile(bundle/'READY',target/'READY')
    os.sync()
    print('Staged and readback-verified. Safely eject USB, then open '+('GleWorks Update Apps' if args.update else 'GleWorks Install')+' in the Kindle library.')
    print('Keep the local backup and the on-device quarantine until all acceptance checks pass.')
if __name__=='__main__':
    try: main()
    except Exception as exc: raise SystemExit(f'Staging stopped: {exc}')
