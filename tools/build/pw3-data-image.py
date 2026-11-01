#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create an empty private data image; never writes to the Kindle."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'lib'))
from pw3_common import digest, require, run, save


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--size-mib', type=int, required=True,
                        help='Explicit measured capacity, 64 to 2048 MiB')
    parser.add_argument('--output', type=Path, required=True, help='New private output directory')
    args = parser.parse_args()
    try:
        build(args)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')


def build(args):
    os.umask(0o077)
    require(64 <= args.size_mib <= 2048, 'Data image size must be 64 to 2048 MiB')
    require(not args.output.exists(), 'Output directory must be new')
    for tool in ['mkfs.ext3', 'debugfs']:
        require(shutil.which(tool), 'Missing host tool: ' + tool)
    args.output.mkdir(parents=True, mode=0o700)
    tree = args.output / 'empty-root'
    tree.mkdir(mode=0o755)
    tree.chmod(0o755)
    for name in ['apps', 'books', 'config']:
        (tree / name).mkdir(mode=0o755)
        (tree / name).chmod(0o755)
    image = args.output / 'data.ext3'
    with image.open('xb') as stream:
        stream.truncate(args.size_mib * 1024 * 1024)
        stream.flush()
        os.fsync(stream.fileno())
    run(['mkfs.ext3', '-q', '-F', '-b', '1024', '-I', '128', '-O',
         'none,has_journal,filetype,sparse_super,large_file', '-L', 'GWData',
         '-m', '0', '-d', tree, image])
    commands = args.output / 'ownership.debugfs'
    commands.write_text(''.join('set_inode_field ' + path + ' ' + field + ' 0\n'
                                for path in ['/', '/apps', '/books', '/config']
                                for field in ['uid', 'gid']))
    run(['debugfs', '-w', '-f', commands, image])
    save(args.output / 'data-image.json', {
        'label': 'GWData', 'size_mib': args.size_mib, 'initial_sha256': digest(image),
        'empty': True, 'device_validated': False,
        'warning': 'Do not mount on PW3 until the matching shutdown and rescue path are verified.',
    })
    print('Private empty data image:', image)
    print('No live data was copied or deleted; size and device use remain unqualified.')


if __name__ == '__main__':
    main()
