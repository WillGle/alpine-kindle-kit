#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract optional private app bundles from an existing native root, offline.

No settings, book history, caches, VPN identity, keys or state are migrated.
The app sources remain unchanged. Runtime requires subsequent PW3 acceptance.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
PRIVATE = {'settings', 'docsettings', 'hashdocsettings', 'cache', 'clipboard', 'ota',
           'history', 'screenshots', 'news', '.git'}


def excluded(path):
    return any(part in PRIVATE or part.startswith(('settings.reader.lua', 'history.lua', 'crash.log',
                                                  'defaults.custom.lua', 'defaults.persistent.lua'))
               or '.before-' in part or part.endswith(('.old', '.bak', '.log', '.sdr'))
               for part in path.parts)


def copy_tree(source, destination):
    skipped = []
    for path in sorted(source.rglob('*')):
        relative = path.relative_to(source)
        if excluded(relative):
            skipped.append(relative.as_posix())
            continue
        # Existing gwapp deliberately rejects links: never silently dereference
        # one into an unrelated host path while assembling a private package.
        if path.is_symlink():
            raise ValueError('App input symlink requires explicit review: ' + str(path))
        if path.is_file():
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
            target.chmod(0o755 if path.stat().st_mode & 0o111 else 0o644)
    return skipped


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--key', required=True, type=Path, help='Explicit private signing key; test key for VM only')
    args = p.parse_args()
    if args.output.exists():
        p.error('output must be new')
    for name in ('opt/koreader/native.lua', 'opt/koreader/luajit',
                 'opt/reader-compat/ld-linux.so.3', 'usr/sbin/tailscaled'):
        path = args.root / name
        if not path.is_file() or path.is_symlink():
            p.error('Missing regular app input: ' + name)
    args.output.mkdir(parents=True, mode=0o700)
    reader = args.output / 'koreader-source'
    tail = args.output / 'tailscale-source'
    skipped = copy_tree(args.root / 'opt/koreader', reader / 'payload/koreader')
    copy_tree(args.root / 'opt/reader-compat', reader / 'payload/compat')
    (tail / 'payload').mkdir(parents=True)
    shutil.copyfile(args.root / 'usr/sbin/tailscaled', tail / 'payload/tailscaled')
    (tail / 'payload/tailscaled').chmod(0o755)
    (reader / 'payload/run').write_text('''#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
export KO_HOME="${GW_APP_DATA:?app data directory required}"
export HOME="$KO_HOME" LANG=C.UTF-8 LC_ALL=C.UTF-8 GW_MANAGED_READER=1
export LD_LIBRARY_PATH="$here/compat:$here/koreader/libs"
cd "$here/koreader"
if [ "${1:-}" = --version ]; then
    exec "$here/compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" ./luajit -v
fi
# The OS/device profile owns input discovery, power and framebuffer setup.
[ -c /dev/fb0 ] && [ -n "${GW_TOUCH_DEV:-}" ] && [ -c "$GW_TOUCH_DEV" ]
exec "$here/compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" ./luajit native.lua /mnt/us/documents
''')
    (tail / 'payload/run').write_text('''#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
if [ "${1:-}" = --version ]; then exec "$here/tailscaled" --version; fi
: "${GW_APP_DATA:?app data directory required}"
exec "$here/tailscaled" --tun=userspace-networking --state="$GW_APP_DATA/tailscaled.state" --socket="$GW_APP_DATA/tailscaled.sock"
''')
    for name, source, formats, permissions in (
            ('koreader', reader, 'epub,pdf', 'display,touch'),
            ('tailscale', tail, '', 'network')):
        (source / 'payload/run').chmod(0o755)
        (source / 'manifest').write_text(
            f'id={name}\nversion=0.1\nentrypoint=payload/run\nformats={formats}\n'
            f'permissions={permissions}\narch=armv7\nprofile=pw3-0kb-5.16.2.1.1\nmin_os=0.1\n')
        subprocess.run([sys.executable, REPO / 'tools/build/gwapp.py', 'build', '--source', source,
                        '--out', args.output / (name + '.gwapp'), '--key', args.key], check=True)
    files = {path.name: {'bytes': path.stat().st_size,
                         'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
             for path in args.output.glob('*.gwapp*')}
    (args.output / 'apps.json').write_text(json.dumps({
        'schema': 1, 'files': files, 'excluded_reader_paths': skipped,
        'migrated_personal_state': False, 'hardware_verified': False,
        'limits': ['Private development exports, version 0.1 is a packaging revision, not upstream version.',
                   'Reader display/input and managed power require PW3 acceptance.',
                   'Tailscale authentication, network transport and concurrent background service lifecycle are not accepted.']}, indent=2) + '\n')
    print('Optional app packages ready; personal state excluded. Hardware acceptance pending.')


if __name__ == '__main__':
    main()
