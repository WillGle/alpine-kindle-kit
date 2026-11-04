#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Static, non-executing compatibility gate for agent-authored ARMv7 apps."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def guest_file(root, name):
    """Resolve guest absolute links in the guest, never against the host root."""
    parts = list(PurePosixPath(name).parts)
    resolved = []
    links = 0
    while parts:
        part = parts.pop(0)
        if part in ('/', '.', ''):
            continue
        if part == '..':
            if not resolved:
                raise ValueError('path escapes root')
            resolved.pop()
            continue
        candidate = root.joinpath(*resolved, part)
        if candidate.is_symlink():
            links += 1
            if links > 40:
                raise ValueError('symlink loop')
            target = PurePosixPath(os.readlink(candidate))
            if target.is_absolute():
                resolved = []
            parts = list(target.parts) + parts
        else:
            resolved.append(part)
    return root.joinpath(*resolved)


def strict_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate JSON field: ' + key)
        result[key] = value
    return result


def check(source, rootfs, manifest, os_version='0.1'):
    source, rootfs = Path(source).resolve(), Path(rootfs).resolve()
    report = {'schema': 1, 'status': 'blocked', 'static_passed': False,
              'qemu_verified': False, 'hardware_verified': False,
              'errors': [], 'inputs': {}, 'pending': ['qemu-lifecycle-and-resources',
              'pw3-kernel-and-cpu', 'pw3-power-and-suspend'],
              'limits': ['No app code is executed. No symbol-version, dlopen or language-import proof.',
                         'Authoring gate only; not a device-side sandbox or installer policy.']}

    def error(code, path, detail):
        report['errors'].append({'code': code, 'path': str(path), 'detail': detail})

    def record(path, root, prefix):
        report['inputs'][prefix + path.relative_to(root).as_posix()] = digest(path)

    try:
        if not source.is_dir() or not rootfs.is_dir():
            raise ValueError('source and extracted rootfs directories are required')
        if (source / 'manifest').stat().st_size > 4096:
            raise ValueError('manifest exceeds 4096 bytes')
        raw_manifest = (source / 'manifest').read_bytes()
        if any(c != 10 and not 32 <= c <= 126 for c in raw_manifest):
            raise ValueError('manifest requires printable ASCII and LF line endings')
        required = {'id': 64, 'version': 32, 'entrypoint': 256, 'arch': 16,
                    'profile': 64, 'min_os': 16, 'formats': None, 'permissions': None}
        if not set(required).issubset(manifest) or set(manifest) - (set(required) | {'description'}):
            raise ValueError('manifest has missing or unknown fields')
        for field, value in manifest.items():
            limit = required.get(field, 128)
            if any(ord(c) < 32 or ord(c) > 126 for c in value) or value != value.strip():
                raise ValueError('manifest contains invalid characters/whitespace')
            if limit and (not value or len(value) >= limit):
                raise ValueError('manifest field exceeds bounds: ' + field)
        if not re.fullmatch(r'[a-z0-9][a-z0-9.-]*', manifest['id']):
            raise ValueError('invalid app ID')
        if not re.fullmatch(r'[A-Za-z0-9._-]+', manifest['version']):
            raise ValueError('invalid app version')
        for field, allowed in [('permissions', {'display', 'touch', 'frontlight', 'network'}),
                               ('formats', {'epub', 'pdf', 'cbz', 'txt', 'html'})]:
            items = manifest[field].split(',') if manifest[field] else []
            if len(set(items)) != len(items) or set(items) - allowed:
                raise ValueError('unknown/duplicate manifest ' + field)
        declaration = source / 'compatibility.json'
        if declaration.is_symlink() or declaration.stat().st_size > 16384:
            raise ValueError('compatibility.json must be a regular JSON file, at most 16 KiB')
        contract = json.loads(declaration.read_text(), object_pairs_hook=strict_object)
        fields = {'schema', 'kind', 'network', 'library_dirs', 'commands', 'kernel_features'}
        if not isinstance(contract, dict) or set(contract) != fields:
            raise ValueError('compatibility.json requires exactly: ' + ', '.join(sorted(fields)))
        if type(contract['schema']) is not int or contract['schema'] != 1:
            raise ValueError('unsupported compatibility schema')
        if contract['kind'] not in ('cli', 'foreground', 'service'):
            raise ValueError('kind must be cli, foreground or service')
        if contract['network'] not in ('none', 'client', 'server'):
            raise ValueError('network must be none, client or server')
        for field in ('library_dirs', 'commands', 'kernel_features'):
            items = contract[field]
            if (not isinstance(items, list) or len(items) > 32 or
                    any(not isinstance(s, str) or not s or len(s) > 255 for s in items) or
                    len(set(items)) != len(items)):
                raise ValueError(field + ' must be a bounded list of unique strings')
        for name in contract['library_dirs']:
            if not re.fullmatch(r'[A-Za-z0-9_+.-]+(?:/[A-Za-z0-9_+.-]+)*', name) or any(
                    p in ('.', '..') for p in PurePosixPath(name).parts):
                raise ValueError('library directory must be a safe relative package path')
            if not (source / name).is_dir():
                raise ValueError('missing private library directory: ' + name)
        for name in contract['commands']:
            if not name.startswith('/') or any(p in ('.', '..') for p in name.split('/')[1:]):
                raise ValueError('commands must be normalized absolute guest paths')
        if contract['kind'] == 'service':
            error('service-lifecycle-unqualified', 'compatibility.json',
                  'Concurrent background apps require a separately qualified supervisor contract.')
        for feature in contract['kernel_features']:
            error('kernel-feature-unverified', feature, 'No qualified target capability profile exists yet.')
        permissions = manifest.get('permissions', '').split(',')
        if contract['network'] != 'none' and 'network' not in permissions:
            error('permission-missing', 'manifest', 'Network app must declare network permission.')
        if contract['network'] == 'server':
            report['pending'] += ['listener-auth-upload-safety', 'listener-stop-and-suspend',
                                  'tailscale-userspace-inbound-if-used']
        if 'display' in permissions or 'touch' in permissions:
            report['pending'].append('pw3-display-touch-and-handoff')
        if manifest.get('arch') != 'armv7' or manifest.get('profile') != 'pw3-0kb-5.16.2.1.1':
            error('target-mismatch', 'manifest', 'Expected armv7 / pw3-0kb-5.16.2.1.1.')

        def version(value):
            if not re.fullmatch(r'\d{1,5}(?:\.\d{1,5}){0,2}', value):
                raise ValueError('invalid OS version')
            result = tuple(int(n) for n in value.split('.'))
            if any(n > 65535 for n in result):
                raise ValueError('OS version component exceeds 65535')
            return result + (0,) * (3 - len(result))
        if version(manifest.get('min_os', '')) > version(os_version):
            error('os-too-old', 'manifest', 'App requires a newer OS version.')

        files = []
        payload_bytes = 0
        for path in sorted(source.rglob('*')):
            if path.is_symlink() or not (path.is_file() or path.is_dir()):
                raise ValueError('package links/special files are not allowed: ' + str(path.relative_to(source)))
            if path.is_file():
                payload_bytes += path.stat().st_size
                if len(files) >= 4096 or payload_bytes > 256 * 1024 * 1024:
                    raise ValueError('core package limits exceeded: 4096 files / 256 MiB expanded')
                relative = path.relative_to(source).as_posix()
                if len(relative) >= 512 or not re.fullmatch(r'[A-Za-z0-9_+.-]+(?:/[A-Za-z0-9_+.-]+)*', relative):
                    raise ValueError('unsafe package member: ' + relative)
                record(path, source, 'app:')
                files.append(path)
        report['payload_bytes'] = payload_bytes
        report['file_count'] = len(files)
        if sum(len(p.relative_to(source).as_posix()) for p in files) > 1024 * 1024:
            raise ValueError('package member names exceed the 1 MiB installer limit')
        entry = manifest.get('entrypoint', '')
        if (not re.fullmatch(r'[A-Za-z0-9_+.-]+(?:/[A-Za-z0-9_+.-]+)*', entry) or
                any(p in ('.', '..') for p in PurePosixPath(entry).parts)):
            raise ValueError('unsafe entrypoint')
        if not (source / entry).is_file() or not os.access(source / entry, os.X_OK):
            raise ValueError('entrypoint must be an executable regular file')
        readelf = os.environ.get('READELF') or shutil.which('readelf')
        if not readelf:
            raise ValueError('readelf not found; enter the build shell or set READELF')
        environment = dict(os.environ, LC_ALL='C')
        seen = set()
        # Explicit app-local paths must match the launcher's LD_LIBRARY_PATH.
        library_dirs = [source / name for name in contract['library_dirs']]
        target_dirs = ['/lib', '/usr/lib', '/usr/local/lib']

        def inspect(path, origin, expected_float=None, role='resource'):
            key = (path, expected_float, role)
            if key in seen:
                return
            seen.add(key)
            if origin == 'target':
                record(path, rootfs, 'target:')
            with path.open('rb') as stream:
                header = stream.read(4096)
            if header.startswith(b'\x7fELF'):
                if len(header) < 52 or header[4:6] != b'\x01\x01' or int.from_bytes(header[18:20], 'little') != 40:
                    error('elf-architecture', path.name, 'Expected ARM ELF32 little-endian; rebuild for ARMv7.')
                    return
                elf_type = int.from_bytes(header[16:18], 'little')
                if elf_type not in (2, 3) or (role == 'library' and elf_type != 3):
                    error('elf-type', path.name, 'Runtime binaries must be EXEC/DYN; shared libraries must be DYN.')
                    return
                flags = int.from_bytes(header[36:40], 'little')
                if flags >> 24 != 5:
                    error('elf-abi', path.name, 'Expected ARM EABI5.')
                float_abi = flags & 0x600
                if float_abi == 0x600:
                    error('elf-float-abi', path.name, 'Conflicting float ABI flags.')
                if expected_float and float_abi != expected_float:
                    error('elf-float-mismatch', path.name, 'Loader/library float ABI differs from its consumer.')
                result = subprocess.run([readelf, '-W', '-l', '-d', str(path)], env=environment,
                                        capture_output=True, text=True, timeout=30)
                if result.returncode:
                    error('elf-malformed', path.name, 'readelf could not inspect this ELF.')
                    return
                text = result.stdout
                if not float_abi and ('Requesting program interpreter:' in text or '(NEEDED)' in text):
                    error('elf-float-abi', path.name, 'Dynamic binary has unspecified float ABI; review/rebuild it.')
                for loader in re.findall(r'Requesting program interpreter: ([^\]]+)\]', text):
                    target = guest_file(rootfs, loader)
                    if not loader.startswith('/') or not target.is_file() or not os.access(target, os.X_OK):
                        error('loader-missing', loader, 'Bundle a reviewed runtime/launcher or rebuild against target musl.')
                    else:
                        inspect(target, 'target', float_abi, role='loader')
                if re.search(r'\((?:RPATH|RUNPATH)\)', text):
                    error('elf-search-path', path.name,
                          'Embedded RPATH/RUNPATH requires review; use explicit app-local library_dirs and launcher.')
                for name in re.findall(r'\(NEEDED\).*?\[([^\]]+)\]', text):
                    if '/' in name:
                        error('library-name', name, 'NEEDED entries must be library basenames.')
                        continue
                    candidates = [(d / name, 'app') for d in library_dirs] + [
                        (guest_file(rootfs, d + '/' + name), 'target') for d in target_dirs]
                    found = next(((p, o) for p, o in candidates if p.is_file()), None)
                    if found is None:
                        error('library-missing', name, 'Add an app-local library or use the target toolchain.')
                    else:
                        inspect(*found, float_abi, role='library')
            elif header.startswith(b'#!') and role not in ('library', 'loader', 'interpreter'):
                line = header.split(b'\n', 1)[0].decode('ascii')
                words = line[2:].split()
                if not words or not words[0].startswith('/') or words[0] == '/usr/bin/env':
                    error('script-interpreter', path.name, 'Use an explicit absolute interpreter; env lookup is not qualified.')
                    return
                interpreter = guest_file(rootfs, words[0])
                if not interpreter.is_file() or not os.access(interpreter, os.X_OK):
                    error('interpreter-missing', words[0], 'Provide a qualified runtime or use /bin/sh.')
                else:
                    inspect(interpreter, 'target', role='interpreter')
            elif role != 'resource' or path == source / entry:
                error('runtime-format', path.name, 'Executable needs ELF/shebang; loader and shared library need ELF.')

        for path in files:
            inspect(path, 'app')
        for command in contract['commands']:
            target = guest_file(rootfs, command)
            if not target.is_file() or not os.access(target, os.X_OK):
                error('command-missing', command, 'Declare/provide the command in the qualified runtime.')
            else:
                inspect(target, 'target', role='executable')
        report['contract'] = contract
    except (ValueError, OSError, UnicodeError, subprocess.SubprocessError) as exc:
        error('invalid-input', 'source/rootfs', str(exc))
    report['static_passed'] = not report['errors']
    report['status'] = 'static-passed' if report['static_passed'] else 'blocked'
    return report
