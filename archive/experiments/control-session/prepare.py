#!/usr/bin/env python3
"""Prepare one temporary, key-only Kindle SSH session; never start it remotely."""
from pathlib import Path
import hashlib
import ipaddress
import json
import subprocess
import sys

base = Path(__file__).resolve().parent
root = base.parent
host = str(ipaddress.IPv4Address(sys.argv[1]))
private = base / '.private'
private.mkdir(mode=0o700, exist_ok=True)
private.chmod(0o700)
client = private / 'client_ed25519'
if not client.exists():
    subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-C',
                    'gleworks-temporary-control', '-f', str(client)], check=True)
keytool = '/nix/store/3qmf80hr38g3c3ajrl3xdfqdfsvphpyf-dropbear-2026.91/bin/dropbearkey'
hostkey = private / 'server_ed25519'
if not hostkey.exists():
    subprocess.run([keytool, '-t', 'ed25519', '-f', str(hostkey)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
public = subprocess.check_output([keytool, '-y', '-f', str(hostkey)], text=True)
public = next(line for line in public.splitlines() if line.startswith('ssh-ed25519 '))
(private / 'known_hosts').write_text('gleworks-temporary-control ' + public + '\n')
inputs = {
    'dropbear': root.parent / 'bundle/koreader/dropbear',
    'libz.so.1': root.parent / 'bundle/koreader/libs/libz.so.1',
    'server-key': hostkey,
    'authorized_keys': client.with_suffix('.pub'),
    'busybox': root / 'alpine-boot/selected/bin/busybox',
    'ld-musl-armhf.so.1': root / 'alpine-boot/selected/lib/ld-musl-armhf.so.1',
    'alpine-release': root / 'alpine-boot/selected/etc/alpine-release',
    'session.sh': base / 'session.sh',
    'gleworks-alpine-stage.sh': root / 'alpine-stage/bundle/gleworks-alpine-stage.sh',
}
expected = {
    'busybox': 'df4dd71f89214032872e60328f3af03faf35e344f6fa5493656d8775d35e930c',
    'ld-musl-armhf.so.1': '2acf3c82bc97b496c40e1d5007d4258570c8df0edc45793d6651a352fb778990',
}
bundle = base / 'bundle'
bundle.mkdir(mode=0o700, exist_ok=True)
bundle.chmod(0o700)
manifest = []
for name, source in inputs.items():
    data = source.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if name in expected:
        assert digest == expected[name], name
    (bundle / name).write_bytes(data)
    (bundle / name).chmod(0o600)
    manifest.append(digest + '  ' + name + '\n')
(bundle / 'SHA256SUMS').write_text(''.join(manifest))
manifest_hash = hashlib.sha256((bundle / 'SHA256SUMS').read_bytes()).hexdigest()
launcher = (base / 'launcher.in.sh').read_text().replace('@HOST@', host)
launcher = launcher.replace('@MANIFEST_HASH@', manifest_hash)
assert '@' not in launcher
subprocess.run(['sh', '-n'], input=launcher, text=True, check=True)
subprocess.run(['sh', '-n', str(base / 'session.sh')], check=True)
(bundle / 'gleworks-alpine-session.sh').write_text(launcher)
(base / 'host-config.json').write_text(json.dumps({'host_ipv4': host,
    'kindle_port': 2222, 'session_seconds': 3600,
    'host_key_alias': 'gleworks-temporary-control',
    'device_session_started': False, 'native_boot_confirmed': False}, indent=2) + '\n')
print('Temporary SSH keys and pinned Alpine ABI diagnostic packaged; no device command executed.')
