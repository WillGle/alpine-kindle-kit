#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build one private candidate twice and require identical payload/image bytes.

Pass the same private inputs as tools/build/pw3-build.py.  The output path is
reused intentionally: a reproducible build must not depend on its directory
name.  Requires the private ARM toolchain, kernel build and captured inputs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BUILDER = ROOT / 'tools/build/pw3-build.py'


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True,
                        help='Directory reused by both builds')
    parser.add_argument('--variant', choices=['base', 'reader'], default='base')
    parser.add_argument('--reader', type=Path)
    parser.add_argument('--work-dir', type=Path, default=ROOT / 'work')
    parser.add_argument('--source-date-epoch', type=int, default=0)
    parser.add_argument('--seed', default='roundtrip')
    parser.add_argument('--cross-prefix', required=True)
    parser.add_argument('--kernel-source', type=Path, required=True)
    parser.add_argument('--kernel-build', type=Path, required=True)
    parser.add_argument('--musl-prefix', type=Path, required=True)
    parser.add_argument('--linux-headers', type=Path, required=True)
    parser.add_argument('--rootfs', type=Path, required=True)
    parser.add_argument('--modules', type=Path, required=True)
    parser.add_argument('--p1-backup', type=Path, required=True)
    parser.add_argument('--return-launcher', type=Path, required=True)
    parser.add_argument('--qemu-arm', default='qemu-arm')
    parser.add_argument('--hostcc', default='cc')
    parser.add_argument('--make', default='make')
    parser.add_argument('--force', action='store_true',
                        help='allow an output path inside the repository or home')
    args = parser.parse_args()

    output = args.output.expanduser().resolve()
    protected_roots = [path for path in (Path.home().resolve(), ROOT.resolve())
                       if path != Path('/')]
    destroys_protected = output == Path('/') or any(
        is_within(output, root) for root in protected_roots)
    if destroys_protected and not args.force:
        parser.error('refusing to delete protected output path %s; pass --force'
                     % output)
    if args.variant == 'reader' and not args.reader:
        parser.error('--reader is required with --variant reader')
    env = dict(os.environ, SOURCE_DATE_EPOCH=str(args.source_date_epoch),
               GW_BUILD_SEED=args.seed)
    command = [
        sys.executable, str(BUILDER),
        '--rootfs', str(args.rootfs), '--modules', str(args.modules),
        '--p1-backup', str(args.p1_backup),
        '--return-launcher', str(args.return_launcher),
        '--cross-prefix', args.cross_prefix,
        '--kernel-source', str(args.kernel_source),
        '--kernel-build', str(args.kernel_build),
        '--musl-prefix', str(args.musl_prefix),
        '--linux-headers', str(args.linux_headers),
        '--qemu-arm', args.qemu_arm, '--hostcc', args.hostcc,
        '--make', args.make, '--variant', args.variant,
        '--output', str(args.output),
    ]
    if args.reader:
        command += ['--reader', str(args.reader)]

    proofs = []
    for _ in range(2):
        shutil.rmtree(args.output, ignore_errors=True)
        result = subprocess.run(command, env=env)
        if result.returncode != 0:
            print('candidate build failed', file=sys.stderr)
            return 1
        candidate = json.loads((args.output / 'candidate.json').read_text())
        proofs.append({
            'files': candidate['files'],
            'system_sha256': digest(args.output / 'system.ext3'),
            'image_mib': candidate['image_mib'],
        })
    if proofs[0]['files'] != proofs[1]['files']:
        print('payload inventory differs between builds', file=sys.stderr)
        return 1
    if proofs[0]['system_sha256'] != proofs[1]['system_sha256']:
        print('image hash differs between builds', file=sys.stderr)
        return 1
    print(json.dumps({
        'variant': args.variant,
        'files_equal': True,
        'system_sha256': proofs[0]['system_sha256'],
        'image_mib': proofs[0]['image_mib'],
        'source_date_epoch': args.source_date_epoch,
        'seed': args.seed,
    }, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
