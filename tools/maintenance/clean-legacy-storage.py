#!/usr/bin/env python3
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
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--erase-legacy-user-content', action='store_true', required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--notes-directory', type=Path, required=True)
parser.add_argument('--history-directory', type=Path, required=True)
args = parser.parse_args()
assert not args.evidence.exists(), 'Keep existing cleanup evidence'
kit.enter_rescue()
rescue = kit.Rescue()
image = rescue.control/'system.ext3'
kit.clean_image(image)
assert not (rescue.control/'boot.enabled').exists()
assert not (rescue.control/'boot.pending').exists()
assert not (rescue.control/'boot.repairing').exists()
assert not (rescue.control/'boot.once').exists()
documents = rescue.mount/'documents'
assert documents.is_dir() and not documents.is_symlink()
guides = ['Alpine-Quickstart.epub', 'Alpine-USB-Guide.txt']
args.notes_directory.mkdir(parents=True, exist_ok=True)
archived = {}
for name in guides:
    source = documents/name
    if not source.exists():
        continue
    assert source.is_file() and not source.is_symlink(), name
    destination = args.notes_directory/name
    if destination.exists():
        assert destination.is_file() and not destination.is_symlink()
        assert kit.digest(destination) == kit.digest(source), name
    else:
        with source.open('rb') as input, destination.open('xb') as output:
            shutil.copyfileobj(input, output)
            output.flush()
            kit.os.fsync(output.fileno())
    assert kit.digest(destination) == kit.digest(source), name
    archived[name] = kit.digest(destination)
kit.syncdir(args.notes_directory)
preserved = [image]
hashes = {str(p.relative_to(rescue.mount)): kit.digest(p) for p in preserved}
args.history_directory.mkdir(parents=True, exist_ok=False)
for source in rescue.control.iterdir():
    if source.name == 'system.ext3' or source.name.startswith('boot.'):
        continue
    rescue.same()
    assert source.is_file() and not source.is_symlink(), source
    destination = args.history_directory/source.name
    with source.open('rb') as input, destination.open('xb') as output:
        shutil.copyfileobj(input, output)
        output.flush()
        kit.os.fsync(output.fileno())
    assert kit.digest(destination) == kit.digest(source), source
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
    assert kit.digest(path) == hashes[str(path.relative_to(rescue.mount))], path
kit.clean_image(image)
after_free = shutil.disk_usage(rescue.mount).free
proof = {
    'scope': 'Legacy shared storage only; p1/p2/p3 not exposed or modified',
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
