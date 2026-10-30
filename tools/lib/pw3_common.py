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


def clean_image(path):
    require(path.is_file() and not path.is_symlink(), 'Root image must be a regular file')
    require(path.stat().st_size == 256*1024*1024, 'Expected 256 MiB root image')
    with path.open('rb') as stream:
        stream.seek(1024)
        sb = stream.read(104)
    require(sb[56:60] == b'\x53\xef\x01\x00' and not sb[96] & 4,
            'Root image requires offline filesystem repair')
    run(['e2fsck', '-fn', path])
