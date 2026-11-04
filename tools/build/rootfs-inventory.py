#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read an extracted rootfs; report actual files separately from apk metadata."""
from __future__ import annotations
import argparse
from collections import defaultdict
import hashlib
import json
import os
from pathlib import Path

DEV = {'gdb', 'musl-dbg', 'python3', 'python3-pyc', 'python3-pycache-pyc0', 'pyc', 'scanelf'}


def package_index(root):
    registry = root / 'lib/apk/db/installed'
    owners, packages = {}, []
    if not registry.is_file():
        return owners, packages
    for block in registry.read_text().split('\n\n'):
        metadata, directory, files = {}, '', []
        for line in block.splitlines():
            key, _, value = line.partition(':')
            if key in ('P', 'V', 'I'):
                metadata[key] = value
            if key == 'F':
                directory = value
            elif key == 'R':
                files.append('/'.join(filter(None, (directory, value))))
        if 'P' not in metadata:
            continue
        present_bytes, missing = 0, 0
        for name in files:
            owners[name] = metadata['P']
            path = root / name
            if path.is_file() and not path.is_symlink():
                present_bytes += path.stat().st_size
            elif not path.exists() and not path.is_symlink():
                missing += 1
        packages.append({'name': metadata['P'], 'version': metadata.get('V'),
                         'apk_declared_bytes': int(metadata.get('I', 0)),
                         'present_regular_file_bytes': present_bytes, 'missing_paths': missing})
    return owners, packages


def category(name, owner):
    if name.startswith(('opt/koreader/', 'opt/reader-compat/')):
        return 'app-koreader'
    if 'tailscale' in name:
        return 'app-tailscale'
    if name.startswith(('var/lib/', 'root/', 'home/')) or name in ('etc/wpa_supplicant.conf',):
        return 'state-or-private-config'
    if '.before-' in name or name.endswith(('.bak', '.old')) or '/backup' in name:
        return 'retained-backup'
    if owner in DEV or name.startswith(('usr/include/', 'usr/lib/debug/', 'usr/lib/python',
                                         'usr/share/man/', 'usr/share/doc/')):
        return 'development-or-diagnostics'
    if name.startswith(('lib/modules/', 'lib/firmware/', 'opt/ar6k/')) or 'wpa_' in name or 'wifi' in name:
        return 'hardware-network-profile'
    if name.startswith(('lib/', 'usr/lib/')) and '.so' in name:
        return 'shared-runtime-review'
    if name.startswith(('bin/', 'sbin/', 'usr/bin/', 'usr/sbin/', 'gleworks/', 'etc/')):
        return 'base-system-and-services'
    return 'other-resources-review'


def inventory(root):
    owners, packages = package_index(root)
    files, groups, seen, unique_bytes = [], defaultdict(lambda: {'bytes': 0, 'files': 0}), set(), 0
    for path in sorted(root.rglob('*')):
        name = path.relative_to(root).as_posix()
        st = path.lstat()
        if path.is_symlink():
            files.append({'path': name, 'type': 'symlink', 'target': os.readlink(path)})
        elif path.is_file():
            group = category(name, owners.get(name))
            groups[group]['bytes'] += st.st_size
            groups[group]['files'] += 1
            inode = (st.st_dev, st.st_ino)
            if inode not in seen:
                unique_bytes += st.st_size
                seen.add(inode)
            files.append({'path': name, 'bytes': st.st_size, 'category': group,
                          'package': owners.get(name)})
    return {'source': str(root), 'regular_path_bytes': sum(g['bytes'] for g in groups.values()),
            'unique_inode_bytes': unique_bytes, 'groups': dict(sorted(groups.items())),
            'packages': packages, 'files': files,
            'limits': 'Logical file sizes; extracted special nodes excluded. apk sizes are metadata, not measured usage. Categories are a review aid, not a dependency proof.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', required=True, type=Path)
    p.add_argument('--image', type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    if args.output.exists():
        p.error('output must be new')
    result = inventory(args.root)
    if args.image:
        with args.image.open('rb') as stream:
            result['source_image_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('files', 'packages')}, indent=2))


if __name__ == '__main__':
    main()
