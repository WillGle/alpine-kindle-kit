#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Delete legacy storage; archive generated guides on the host, keep native root."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse
import importlib
import json
import shutil

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
kit = importlib.import_module('kindle-linux')
require = kit.require
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--erase-legacy-user-content', action='store_true', required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--notes-directory', type=Path, required=True)
parser.add_argument('--history-directory', type=Path, required=True)
parser.add_argument('--dry-run', action='store_true',
                    help='print the deletion inventory and stop without writing')
parser.add_argument('--yes', action='store_true',
                    help='confirm the destructive inventory')
args = parser.parse_args()
require(args.dry_run or args.yes,
        'Destructive cleanup requires --yes (or use --dry-run to inspect)')
require(not args.evidence.exists(), 'Keep existing cleanup evidence')

kit.enter_rescue()
with kit.Rescue() as rescue:
    image = rescue.control/'system.ext3'
    kit.clean_image(image)
    require(not (rescue.control/'boot.enabled').exists())
    require(not (rescue.control/'boot.pending').exists())
    require(not (rescue.control/'boot.repairing').exists())
    require(not (rescue.control/'boot.once').exists())
    documents = rescue.mount/'documents'
    require(documents.is_dir() and not documents.is_symlink())

    def entry_bytes(path):
        if path.is_symlink() or path.is_file():
            return path.lstat().st_size
        return sum(p.lstat().st_size for p in path.rglob('*')
                   if p.is_file() or p.is_symlink())

    inventory = {
        'root_entries': sorted(
            p.name for p in rescue.mount.iterdir()
            if p.name not in {'documents', 'gleworks-alpine'}),
        'document_entries': sorted(p.name for p in documents.iterdir()),
        'control_entries': sorted(
            p.name for p in rescue.control.iterdir()
            if p.name != 'system.ext3'),
        'estimated_bytes': sum(
            entry_bytes(p) for p in rescue.mount.iterdir()
            if p.name not in {'gleworks-alpine'}),
    }
    print(json.dumps({'dry_run': bool(args.dry_run), 'inventory': inventory},
                     indent=2))
    if args.dry_run:
        print('Dry run: no files were removed.')
        sys.exit(0)

    guides = ['Alpine-Quickstart.epub', 'Alpine-USB-Guide.txt']
    args.notes_directory.mkdir(parents=True, exist_ok=True)
    archived = {}
    for name in guides:
        source = documents/name
        if not source.exists():
            continue
        require(source.is_file() and not source.is_symlink(), name)
        destination = args.notes_directory/name
        if destination.exists():
            require(destination.is_file() and not destination.is_symlink())
            require(kit.digest(destination) == kit.digest(source), name)
        else:
            with source.open('rb') as input, destination.open('xb') as output:
                shutil.copyfileobj(input, output)
                output.flush()
                kit.os.fsync(output.fileno())
        require(kit.digest(destination) == kit.digest(source), name)
        archived[name] = kit.digest(destination)
    kit.syncdir(args.notes_directory)
    preserved = [image]
    hashes = {str(p.relative_to(rescue.mount)): kit.digest(p) for p in preserved}
    args.history_directory.mkdir(parents=True, exist_ok=False)
    for source in rescue.control.iterdir():
        if source.name == 'system.ext3' or source.name.startswith('boot.'):
            continue
        rescue.same()
        require(source.is_file() and not source.is_symlink(), source)
        destination = args.history_directory/source.name
        with source.open('rb') as input, destination.open('xb') as output:
            shutil.copyfileobj(input, output)
            output.flush()
            kit.os.fsync(output.fileno())
        require(kit.digest(destination) == kit.digest(source), source)
    kit.syncdir(args.history_directory)
    before_free = shutil.disk_usage(rescue.mount).free
    removed = 0

    def remove(path):
        global removed
        rescue.same()
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        else:
            path.unlink()
        removed += 1

    # The root image is offline here: never export its backing FAT while mounted.
    for path in list(rescue.mount.iterdir()):
        if path.name not in {'documents', 'gleworks-alpine'}:
            remove(path)
    for path in list(documents.iterdir()):
        remove(path)
    for path in list(rescue.control.iterdir()):
        if path.name != 'system.ext3':
            remove(path)
    for directory in [documents, rescue.control, rescue.mount]:
        kit.syncdir(directory)
    for path in preserved:
        require(kit.digest(path) == hashes[str(path.relative_to(rescue.mount))], path)
    kit.clean_image(image)
    after_free = shutil.disk_usage(rescue.mount).free
    proof = {
        'scope': 'Legacy shared storage only; p1/p2/p3 not exposed or modified',
        'inventory': inventory,
        'removed_entries': removed,
        'free_bytes_before': before_free,
        'free_bytes_after': after_free,
        'reclaimed_bytes': after_free-before_free,
        'retained_file_sha256': hashes,
        'guides_archived_on_host_sha256': archived,
        'native_root_clean_and_unchanged': True,
    }
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(proof, indent=2)+'\n')
    print(json.dumps(proof, indent=2))
    kit.resume_rescue(rescue)
