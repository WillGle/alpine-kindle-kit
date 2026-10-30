#!/usr/bin/env python3
"""Fetch official pinned releases and produce an integrity-checked USB bundle."""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import ssl
import stat
import tarfile
import urllib.parse
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {'api.github.com', 'github.com', 'objects.githubusercontent.com',
           'release-assets.githubusercontent.com', 'pkgs.tailscale.com'}

class OfficialRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        validate_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)

def validate_url(url):
    p = urllib.parse.urlsplit(url)
    if p.scheme != 'https' or p.hostname not in ALLOWED or p.username or p.password:
        raise ValueError('Unexpected download origin: ' + url)

def tls_context():
    # Standalone Python may look for cert.pem while NixOS provides this bundle.
    # Explicit trust settings retain precedence over system fallback paths.
    if 'SSL_CERT_FILE' in os.environ or 'SSL_CERT_DIR' in os.environ:
        return ssl.create_default_context()
    if os.environ.get('NIX_SSL_CERT_FILE'):
        return ssl.create_default_context(cafile=os.environ['NIX_SSL_CERT_FILE'])
    context = ssl.create_default_context()
    bundle = Path('/etc/ssl/certs/ca-certificates.crt')
    if ssl.get_default_verify_paths().cafile is None and bundle.is_file():
        context.load_verify_locations(cafile=str(bundle))
    return context

def fetch(url, target, limit=512 * 1024 * 1024):
    validate_url(url)
    request = urllib.request.Request(url, headers={'User-Agent': 'GleWorks-Kindle-prepare', 'Accept': 'application/vnd.github+json' if 'api.github.com' in url else '*/*'})
    https = urllib.request.HTTPSHandler(context=tls_context())
    with urllib.request.build_opener(OfficialRedirect(), https).open(request, timeout=30) as response, target.open('xb') as out:
        count = 0
        while data := response.read(1024 * 1024):
            count += len(data)
            if count > limit:
                raise ValueError('Download exceeds size limit')
            out.write(data)

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def arm_elf(path):
    with path.open('rb') as stream: header=stream.read(20)
    if len(header)<20 or header[:6]!=b'\x7fELF\x01\x01' or int.from_bytes(header[18:20],'little')!=40:
        raise ValueError('Expected ARM32 little-endian ELF: '+str(path))

@contextmanager
def archive_contents(archive):
    if not zipfile.is_zipfile(archive):
        with tarfile.open(archive, 'r:gz') as tar:
            yield tar.getmembers(), tar.extractfile
        return
    with zipfile.ZipFile(archive) as zip_archive:
        members, entries = [], {}
        for entry in zip_archive.infolist():
            if entry.orig_filename != entry.filename:
                raise ValueError('Ambiguous ZIP filename')
            member = tarfile.TarInfo(entry.filename)
            member.size = entry.file_size
            mode = entry.external_attr >> 16
            member.mode = mode & 0o777
            if stat.S_ISLNK(mode):
                if entry.file_size > 4096:
                    raise ValueError('Oversized ZIP symlink')
                member.type = tarfile.SYMTYPE
                member.linkname = zip_archive.read(entry).decode('utf-8')
            elif entry.is_dir():
                member.type = tarfile.DIRTYPE
            elif stat.S_IFMT(mode) not in (0, stat.S_IFREG):
                raise ValueError('Unsupported ZIP member')
            members.append(member)
            entries[id(member)] = entry
        yield members, lambda member: zip_archive.open(entries[id(member)])

def extract(archive, destination, prefix, ignored_roots=()):
    """Extract reviewed members ourselves; never follow archive link parents."""
    destination.mkdir()
    with archive_contents(archive) as (members, read_member):
        if len(members) > 30000 or sum(max(0, m.size) for m in members) > 512 * 1024 * 1024:
            raise ValueError('Archive exceeds extraction limits')
        seen = set()
        checked = []
        for m in members:
            if any(ord(c)<32 or c in '\\:*?"<>|' for c in m.name): raise ValueError('Unsafe FAT archive name')
            path = PurePosixPath(m.name)
            if path.is_absolute() or '..' in path.parts or not path.parts:
                raise ValueError('Unexpected archive path: ' + m.name)
            if path.parts[0] in ignored_roots:
                continue
            if path.parts[0] != prefix:
                raise ValueError('Unexpected archive path: ' + m.name)
            relative = Path(*path.parts[1:])
            if relative == Path('.'):
                if not m.isdir(): raise ValueError('Archive root must be directory')
                continue
            if relative in seen: raise ValueError('Duplicate archive member')
            seen.add(relative)
            if not (m.isfile() or m.isdir() or m.issym()): raise ValueError('Unsupported archive member')
            if m.issym():
                link = PurePosixPath(m.linkname)
                target = relative.parent / Path(*link.parts)
                normalized = Path(os.path.normpath(str(target)))
                if link.is_absolute() or '..' in normalized.parts: raise ValueError('Unsafe archive symlink')
            checked.append((m, relative))
        linkpaths = {p for m, p in checked if m.issym()}
        for m, p in checked:
            if any(parent in linkpaths for parent in p.parents): raise ValueError('Archive member below symlink')
        for m, p in checked:
            target = destination / p
            target.parent.mkdir(parents=True, exist_ok=True)
            if m.isdir(): target.mkdir(exist_ok=True)
            elif m.isfile():
                with read_member(m) as src, target.open('xb') as out: shutil.copyfileobj(src, out)
                target.chmod(0o755 if m.mode & 0o111 else 0o644)
        for m, p in checked:
            if m.issym(): (destination / p).symlink_to(m.linkname)
        # Resolve every link, including chains; reject dangling and cyclic links.
        for p in linkpaths:
            resolved = (destination / p).resolve(strict=True)
            if not resolved.is_relative_to(destination.resolve()): raise ValueError('Escaping archive link')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tailscale-version', default='latest', help='Pinned official stable version, e.g. 1.104.4')
    parser.add_argument('--koreader-tag', default='latest', help='Pinned official release tag, e.g. v2026.07')
    parser.add_argument('--koreader-asset', help='Exact kindlepw2 ZIP or tar.gz release asset for PW3')
    parser.add_argument('--koreader-sha256', help='Published digest if GitHub release metadata has no SHA256 digest')
    parser.add_argument('--output', type=Path, default=ROOT / 'bundle')
    args = parser.parse_args()
    if args.tailscale_version != 'latest' and not re.fullmatch(r'\d+\.\d+\.\d+', args.tailscale_version): parser.error('Invalid Tailscale version')
    if args.koreader_tag != 'latest' and not re.fullmatch(r'v[\w.\-]+', args.koreader_tag): parser.error('Invalid KOReader tag')
    if args.koreader_asset and not re.fullmatch(r'koreader-kindlepw2-[\w.\-]+\.(?:zip|tar\.gz)', args.koreader_asset):
        parser.error('PW3 requires the upstream kindlepw2 ZIP or tar.gz asset; specify its exact name')
    output = args.output.absolute()
    if output.exists(): parser.error('Output exists; use a fresh output path')
    output.parent.mkdir(parents=True, exist_ok=True)
    work = output.with_name(output.name + '.incomplete')
    work.mkdir()
    cache = work / 'downloads'; cache.mkdir()
    bundle = work / 'payload'; bundle.mkdir()
    gle = bundle / 'gleworks'; gle.mkdir()
    try:
        version = args.tailscale_version
        if version == 'latest':
            fetch('https://pkgs.tailscale.com/stable/?mode=json', cache / 'tailscale-release.json', 4 * 1024 * 1024)
            version = json.loads((cache / 'tailscale-release.json').read_text())['TarballsVersion']
            if not re.fullmatch(r'\d+\.\d+\.\d+', version): raise ValueError('Invalid official Tailscale release version')
        tsurl = f'https://pkgs.tailscale.com/stable/tailscale_{version}_arm.tgz'
        fetch(tsurl + '.sha256', cache / 'tailscale.sha256', 4096)
        expected = (cache / 'tailscale.sha256').read_text().split()[0].lower()
        if not re.fullmatch('[0-9a-f]{64}', expected): raise ValueError('Invalid official Tailscale checksum')
        fetch(tsurl, cache / 'tailscale.tgz')
        if digest(cache / 'tailscale.tgz') != expected: raise ValueError('Tailscale checksum mismatch')
        ts = work / 'tailscale'; extract(cache / 'tailscale.tgz', ts, f'tailscale_{version}_arm')
        endpoint = 'latest' if args.koreader_tag == 'latest' else 'tags/' + args.koreader_tag
        fetch('https://api.github.com/repos/koreader/koreader/releases/' + endpoint, cache / 'koreader-release.json', 4 * 1024 * 1024)
        release = json.loads((cache / 'koreader-release.json').read_text())
        if args.koreader_tag == 'latest': args.koreader_tag = release['tag_name']
        if not re.fullmatch(r'v[\w.\-]+', args.koreader_tag): raise ValueError('Invalid official KOReader tag')
        if not args.koreader_asset:
            for extension in ('zip', 'tar.gz'):
                name = f'koreader-kindlepw2-{args.koreader_tag}.{extension}'
                candidates = [a['name'] for a in release['assets'] if a['name'] == name]
                if len(candidates) == 1:
                    args.koreader_asset = name
                    break
            if not args.koreader_asset:
                names = ', '.join(a['name'] for a in release['assets'] if 'kindle' in a['name'])
                raise ValueError(f'No matching kindlepw2 ZIP/tar.gz for {args.koreader_tag}. Kindle assets: {names}')
        if release.get('draft') or release.get('prerelease') or release.get('tag_name') != args.koreader_tag:
            raise ValueError('Expected the pinned stable KOReader release')
        asset = next((a for a in release['assets'] if a['name'] == args.koreader_asset), None)
        if not asset: raise ValueError('Exact KOReader asset not found in official release')
        upstream_digest = asset.get('digest') or ''
        koexpected = upstream_digest.removeprefix('sha256:') if upstream_digest.startswith('sha256:') else args.koreader_sha256
        if not koexpected or not re.fullmatch('[0-9a-f]{64}', koexpected):
            raise ValueError('No published SHA256 available. Supply --koreader-sha256 from an independently checked official release checksum')
        if args.koreader_sha256 and args.koreader_sha256 != koexpected: raise ValueError('Conflicting KOReader digests')
        kourl = asset['browser_download_url']
        expected_url = f'https://github.com/koreader/koreader/releases/download/{args.koreader_tag}/{args.koreader_asset}'
        if kourl != expected_url: raise ValueError('Unexpected KOReader release URL')
        koarchive = cache / args.koreader_asset
        fetch(kourl, koarchive)
        if digest(koarchive) != koexpected: raise ValueError('KOReader checksum mismatch')
        # Official Kindle ZIPs include KUAL integration; we supply our own launchers.
        extract(koarchive, bundle / 'koreader', 'koreader', ignored_roots=('extensions',))
        for name in ('luajit', 'reader.lua', 'koreader.sh', 'setupkoenv.lua', 'libs/libkoreader-lfs.so', 'common/socket/score.so'):
            if not (bundle / 'koreader' / name).is_file(): raise ValueError('Missing KOReader runtime member: ' + name)
        shutil.copytree(ROOT / 'plugins' / 'gleworks.koplugin', bundle / 'koreader/plugins/gleworks.koplugin')
        arm_elf(bundle / 'koreader/luajit')
        (gle / 'bin').mkdir()
        for name in ('tailscale', 'tailscaled'):
            arm_elf(ts / name)
            shutil.copy2(ts / name, gle / 'bin' / name)
            (gle / 'bin' / name).chmod(0o755)
        for directory in ('device', 'web', 'scriptlets'):
            shutil.copytree(ROOT / directory, gle / directory)
        (gle / 'scripts').mkdir()
        for name in ('kindle-services.sh', 'show-message.sh', 'record-failure.sh', 'apply-bundle.sh', 'install.sh', 'rollback.sh', 'gleworks-inspect.sh'):
            shutil.copy2(ROOT / 'scripts' / name, gle / 'scripts' / name)
        provenance = {'tailscale': {'version': version, 'url': tsurl, 'sha256': expected},
                      'koreader': {'tag': args.koreader_tag, 'url': kourl, 'sha256': koexpected},
                      'limits': 'Official origin and integrity verified; kernel/runtime/device acceptance is separate.'}
        (gle / 'UPSTREAM.json').write_text(json.dumps(provenance, indent=2) + '\n')
        # Hash dereferenced links too: the VFAT staging copy uses regular files.
        hashes = []
        for p in sorted(bundle.rglob('*')):
            if p.is_file(): hashes.append(f'{digest(p)}  {p.relative_to(bundle).as_posix()}\n')
        shutil.copy2(ROOT / 'scripts' / 'gleworks-install.sh', bundle / 'gleworks-install.sh')
        shutil.copy2(ROOT / 'scripts' / 'rollback.sh', bundle / 'gleworks-rollback.sh')
        # Include the entrypoints in the manifest as well.
        for name in ('gleworks-install.sh', 'gleworks-rollback.sh'):
            hashes.append(f'{digest(bundle / name)}  {name}\n')
        (bundle / 'SHA256SUMS').write_text(''.join(hashes))
        (bundle / 'READY').write_text('GleWorks Kindle bundle v1\n')
        bundle.rename(output)
        print(f'Prepared {output}; {len(hashes)} verified files. No device changes made.')
    finally:
        # Failed output is never installable; remove downloaded working data.
        shutil.rmtree(work)

if __name__ == '__main__':
    try: main()
    except Exception as exc: raise SystemExit(f'Preparation failed: {exc}')
