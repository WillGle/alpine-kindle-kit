#!/usr/bin/env python3
"""Archive and verify unused reader translations on the host before USB removal."""
from pathlib import Path
import argparse
import hashlib
import json
import shlex
import sys
sys.dont_write_bytecode = True
import tarfile

base = Path(__file__).resolve().parent
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from usb_console import Console

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reader', type=Path, required=True, help='Clean host KOReader bundle used as translation reference')
parser.add_argument('--archive', type=Path, required=True)
args = parser.parse_args()
source = args.reader/'l10n'
selected = sorted(p for p in source.iterdir() if p.is_dir() and p.name not in {'vi', 'en_GB'})
expected = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
            for folder in selected for p in folder.rglob('*') if p.is_file()}
assert expected and not args.archive.exists()
console = Console()
try:
    directories = ' '.join(shlex.quote(p.name) for p in selected)
    output = console.run('set -eu\ncd /opt/koreader/l10n\nfind '+directories+' -type f -exec sha256sum {} \\;')
    observed = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 2 and len(fields[0]) == 64 and fields[1] in expected:
            observed[fields[1]] = fields[0]
    assert observed == expected, 'Live translations differ from host archive inputs'
    args.archive.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(args.archive, 'w:gz') as archive:
        for folder in selected:
            archive.add(folder, arcname=folder.name)
    args.archive.chmod(0o600)
    with tarfile.open(args.archive, 'r:gz') as archive:
        archived = {m.name: hashlib.sha256(archive.extractfile(m).read()).hexdigest()
                    for m in archive.getmembers() if m.isfile()}
    assert archived == expected
    script = '''set -eu
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(cat /etc/alpine-release)" = 3.24.3 ]
touch /run/reader-stopped
for path in /proc/[0-9]*/exe; do
 if [ "$(readlink "$path" 2>/dev/null || :)" = /opt/reader-compat/ld-linux.so.3 ]; then
 pid=${path#/proc/}; kill -TERM "${pid%/exe}"
 fi
done
sleep 3
for path in /proc/[0-9]*/exe; do
 [ "$(readlink "$path" 2>/dev/null || :)" != /opt/reader-compat/ld-linux.so.3 ]
done
cd /opt/koreader/l10n
du -sk .
'''
    script += 'rm -r -- '+directories+'\n'
    script += 'du -sk .\nsync\nrm /run/reader-stopped\n'
    print(console.run(script))
finally:
    console.close()
report = {'archive': str(args.archive.resolve()), 'verified_files': len(expected),
          'archived_file_bytes': sum((source/name).stat().st_size for name in expected),
          'kept_ui_languages': ['vi', 'en_GB', 'built-in English'], 'sha256': expected}
args.archive.with_suffix('.review.json').write_text(json.dumps(report, indent=2)+'\n')
print('Archived, hash-verified and removed', len(expected), 'translation files.')
