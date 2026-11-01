#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package only reviewed PW3 installer source/docs; never include a device image or private input."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse
import fnmatch
import gzip
import hashlib
import io
import json
import os
import tarfile
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from pw3_common import ROOT, digest, require

MANIFEST = Path(__file__).with_name('source-manifest.json')
EPOCH = int(os.environ.get('SOURCE_DATE_EPOCH', '0'))


def load_manifest():
    data = json.loads(MANIFEST.read_text())
    require(isinstance(data.get('roots'), dict) and data.get('docs'), 'Invalid source manifest')
    return data


def collect(manifest):
    excluded = manifest.get('exclude', [])
    files = {}
    for directory, patterns in manifest['roots'].items():
        base = ROOT/directory
        require(base.is_dir(), 'Missing source directory: '+directory)
        paths = set()
        for pattern in patterns:
            paths.update(base.glob(pattern))
        for path in sorted(paths):
            if not path.is_file() or path.is_symlink():
                continue
            rel = path.relative_to(ROOT).as_posix()
            if any(fnmatch.fnmatch(rel, pattern) for pattern in excluded):
                continue
            files[rel] = path
    for rel in manifest['docs']:
        path = ROOT/rel
        require(path.is_file(), 'Missing public document: '+rel)
        files[rel] = path
    files['tools/package/source-manifest.json'] = MANIFEST
    for rel in manifest.get('required', []):
        require(rel in files, 'Dependency manifest omits required builder input: '+rel)
    require(all('reset-token.h' not in name and '.private' not in name
                and not name.startswith('work/') and not name.startswith('dist/')
                for name in files), 'Private source member in package')
    return files


def build_archive(files, stream):
    """Deterministic gzip tar: sorted names, fixed metadata and mtime=0."""
    manifest = {name: digest(path) for name, path in files.items()}
    with gzip.GzipFile(fileobj=stream, mode='wb', mtime=0) as gz:
        with tarfile.open(fileobj=gz, mode='w', format=tarfile.GNU_FORMAT) as archive:
            for name, path in sorted(files.items()):
                data = path.read_bytes()
                if name.endswith(('.py', '.sh')):
                    mode = 0o755
                else:
                    mode = 0o644
                info = tarfile.TarInfo('kindle-native/'+name)
                info.size = len(data)
                info.mode = mode
                info.uid = info.gid = 0
                info.uname = info.gname = ''
                info.mtime = EPOCH
                archive.addfile(info, io.BytesIO(data))
            data = (json.dumps(manifest, indent=2, sort_keys=True)+'\n').encode()
            info = tarfile.TarInfo('kindle-native/SOURCE-SHA256.json')
            info.size = len(data)
            info.mode = 0o644
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            info.mtime = EPOCH
            archive.addfile(info, io.BytesIO(data))
    return manifest


def roundtrip(files):
    """Build twice, extract, and verify the published inventory exactly."""
    hashes = []
    expected = sorted('kindle-native/'+name for name in files)
    for _ in range(2):
        with tempfile.TemporaryDirectory() as tmp:
            package = Path(tmp)/'source.tar.gz'
            with package.open('xb') as stream:
                build_archive(files, stream)
            hashes.append(digest(package))
            extract = Path(tmp)/'extract'
            extract.mkdir()
            with tarfile.open(package, 'r:gz') as archive:
                for member in archive:
                    rel = Path(member.name)
                    require(not rel.is_absolute() and '..' not in rel.parts,
                            'Unsafe public archive path: '+member.name)
                    require(member.isfile(), 'Public archive contains a non-file')
                    target = extract/rel
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with target.open('xb') as out:
                        out.write(archive.extractfile(member).read())
            published = json.loads((extract/'kindle-native/SOURCE-SHA256.json').read_text())
            for name, wanted in published.items():
                require(digest(extract/'kindle-native'/name) == wanted,
                        'Extracted source hash mismatch: '+name)
            extracted = sorted(
                str(p.relative_to(extract)).replace(os.sep, '/')
                for p in (extract/'kindle-native').rglob('*') if p.is_file())
            require(extracted == sorted(expected + ['kindle-native/SOURCE-SHA256.json']),
                    'Extracted inventory differs from the dependency manifest')
    require(hashes[0] == hashes[1],
            'Two public-source builds produced different archives')
    return hashes[0], len(files)


def package(output):
    files = collect(load_manifest())
    require(not output.exists(), 'Existing release retained; output must be new')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream:
        manifest = build_archive(files, stream)
    checksum = digest(output)
    output.with_name(output.name+'.sha256').write_text(checksum+'  '+output.name+'\n')
    print('Public-source candidate:', output, 'files:', len(files), 'sha256:', checksum)
    print('No upload performed. Source checks do not certify a fresh physical first installation.')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, nargs='?', default=None)
    parser.add_argument('--check', action='store_true',
                        help='build twice, extract and verify the dependency inventory')
    args = parser.parse_args()
    try:
        files = collect(load_manifest())
        if args.check:
            checksum, count = roundtrip(files)
            print('Public-source roundtrip: PASS files=%d sha256=%s' % (count, checksum))
            return
        output = args.output or ROOT/'dist/pw3-source.tar.gz'
        package(output)
    except (ValueError, OSError, tarfile.TarError) as error:
        parser.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
