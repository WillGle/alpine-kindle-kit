#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compose a private Alpine core for QEMU. No device or mount operations."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import importlib.util

REPO = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pw3_build", REPO / "tools/build/pw3-build.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def hardfloat(path):
    with path.open('rb') as stream:
        header = stream.read(52)
    return (len(header) == 52 and header[:6] == b'\x7fELF\x01\x01' and
            int.from_bytes(header[18:20], 'little') == 40 and
            int.from_bytes(header[36:40], 'little') & 0x600 == 0x400)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rootfs', type=Path, required=True)
    parser.add_argument('--binaries', type=Path, required=True)
    parser.add_argument('--shell', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.absolute()
    builder.safe_path(out)
    if out.exists():
        parser.error('output must be new; old images are retained')
    source = json.loads((REPO / 'os/alpine/source.json').read_text())
    if sha(args.rootfs) != source['sha256']:
        parser.error('pinned Alpine archive hash mismatch')
    for binary in [args.shell, *(args.binaries / n for n in ('gw-system', 'gwctl', 'gw-appctl'))]:
        if not hardfloat(binary):
            parser.error('core binary must be ARM hard-float: ' + str(binary))
    out.mkdir(parents=True)
    root = out / 'root'
    root.mkdir()
    builder.unpack(args.rootfs, root)
    (root / 'sbin/init').unlink()
    (root / 'sbin/init').symlink_to('../bin/busybox')
    for name in ('old-root', 'boot/gleworks-control', 'etc/gleworks/app-keys',
                 'var/lib/gleworks/apps', 'var/lib/gleworks/appdata'):
        (root / name).mkdir(parents=True, exist_ok=True)
    for path in (REPO / 'os/alpine/core').iterdir():
        shutil.copyfile(path, root / 'etc' / path.name)
        (root / 'etc' / path.name).chmod(0o755 if path.suffix == '.sh' else 0o644)
    for name in ('gw-system', 'gwctl', 'gw-appctl'):
        shutil.copyfile(args.binaries / name, root / 'usr/sbin' / name)
        (root / 'usr/sbin' / name).chmod(0o755)
    shutil.copyfile(args.shell, root / 'usr/sbin/gw-shell')
    (root / 'usr/sbin/gw-shell').chmod(0o755)
    shutil.copyfile(args.binaries / 'release.pem', root / 'etc/gleworks/app-keys/release.pem')
    inventory = []
    for path in sorted(root.rglob('*')):
        rel = path.relative_to(root).as_posix()
        if path.is_symlink():
            inventory.append({'path': rel, 'link': os.readlink(path)})
        elif path.is_file():
            inventory.append({'path': rel, 'bytes': path.stat().st_size, 'sha256': sha(path)})
    image = out / 'core.ext3'
    # Headroom for app installation is separate from the measured core payload.
    with image.open('xb') as stream:
        stream.truncate(64 * 1024 * 1024)
    subprocess.run(['mkfs.ext3', '-q', '-F', '-m', '0', '-O', '^64bit,^metadata_csum',
                    '-L', 'AlpineCoreVM', '-d', str(root), str(image)], check=True)
    # mkfs -d preserves host ownership; image trust roots must belong to root.
    commands = []
    for path in [root, *sorted(root.rglob('*'))]:
        rel = '/' if path == root else '/' + path.relative_to(root).as_posix()
        for field in ('uid', 'gid'):
            commands.append(f'set_inode_field {rel} {field} 0')
    commands += ['cd /dev', 'mknod console c 5 1', 'mknod null c 1 3',
                 'set_inode_field console mode 020600', 'set_inode_field null mode 020666']
    command_file = out / 'image.debugfs'
    command_file.write_text('\n'.join(commands) + '\n')
    subprocess.run(['debugfs', '-w', '-f', str(command_file), str(image)],
                   check=True, capture_output=True)
    subprocess.run(['e2fsck', '-fn', str(image)], check=True, capture_output=True)
    report = {'profile': 'qemu-virt-armv7', 'hardware_verified': False,
              'alpine_archive_sha256': sha(args.rootfs), 'image_sha256': sha(image),
              'image_bytes': image.stat().st_size,
              'payload_bytes': sum(x.get('bytes', 0) for x in inventory),
              'excluded_apps': ['koreader', 'tailscale'],
              'inventory': inventory}
    (out / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'inventory'}, indent=2))


if __name__ == '__main__':
    main()
