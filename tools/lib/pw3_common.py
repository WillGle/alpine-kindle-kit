# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared host-side PW3 installer checks. No device operations on import."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
ALPINE = ROOT/'os/alpine'
DEVICE = ROOT/'os/devices/pw3'
READER = ROOT/'apps/koreader'
TOOLS = ROOT/'tools'
ALPINE_SOURCE = json.loads((ALPINE/'source.json').read_text())
DEVICE_PROFILE = json.loads((DEVICE/'profile.json').read_text())
KERNEL = DEVICE_PROFILE['kernel']
STOCK_INIT = DEVICE_PROFILE['stock_init_sha256']
ALPINE_SHA = ALPINE_SOURCE['sha256']


def add_work_dir(parser):
    parser.add_argument('--work-dir', type=Path, default=ROOT/'work',
                        help='Local workspace, defaults to repository work/; may be outside the repo')


def workspace(path):
    return path.expanduser().absolute()


TOKEN = b'gleworks-alpine-v1\n'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    h = hashlib.sha256()
    require(path.is_file() and not path.is_symlink(), 'Expected regular file: '+str(path))
    with path.open('rb') as stream:
        while data := stream.read(1024*1024):
            h.update(data)
    return h.hexdigest()


def run(argv, **kw):
    return subprocess.run([str(x) for x in argv], check=True, capture_output=True, **kw)


def save(path, value):
    partial = path.with_name(path.name+'.partial')
    with partial.open('x') as stream:
        os.chmod(partial, 0o600)
        json.dump(value, stream, indent=2)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(partial, path)
    syncdir(path.parent)


def syncdir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


IMAGE_MIN_MIB = 128
IMAGE_MAX_MIB = 1024
IMAGE_HEADROOM_MIB = 32


def image_size_mib(tree):
    """Pick the smallest safe ext3 image size for a prepared root tree.

    The root is a loop-mounted ext3 file on the FAT userstore, so every unused
    block in the image is storage the reader cannot use for books.  Keep 15%
    plus a fixed headroom for ext3 metadata, journal and runtime state instead
    of always reserving 256 MiB.
    """
    require(tree.is_dir() and not tree.is_symlink(), 'Root tree must be a directory')
    used = sum(p.stat().st_size for p in tree.rglob('*') if p.is_file())
    mib = (used + used // 4 + IMAGE_HEADROOM_MIB * 1024 * 1024 + (1 << 20) - 1) >> 20
    mib = max(IMAGE_MIN_MIB, (mib + 7) // 8 * 8)
    require(mib <= IMAGE_MAX_MIB,
            'Root tree needs %d MiB; maximum supported image is %d MiB' % (mib, IMAGE_MAX_MIB))
    return mib


def clean_image(path):
    require(path.is_file() and not path.is_symlink(), 'Root image must be a regular file')
    require(64*1024*1024 <= path.stat().st_size <= IMAGE_MAX_MIB*1024*1024,
            'Unsupported root image size')
    with path.open('rb') as stream:
        stream.seek(1024)
        sb = stream.read(104)
    require(sb[56:60] == b'\x53\xef\x01\x00' and not sb[96] & 4,
            'Root image requires offline filesystem repair')
    run(['e2fsck', '-fn', path])
