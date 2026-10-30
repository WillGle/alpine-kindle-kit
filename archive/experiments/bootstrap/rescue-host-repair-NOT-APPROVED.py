#!/usr/bin/env python3
"""Supervised USB rescue proof: repair only the known invalid Alpine image."""
from pathlib import Path
import hashlib
import json
import os
import re
import secrets
import stat
import subprocess
import time

base = Path(__file__).resolve().parent
linux = base.parent
image = linux / 'native-rootfs-v5-diagnostic/alpine-diagnostic.ext3'
review = json.loads((image.parent / 'review.json').read_text())
assert review['physical_verified']['usb_fresh_nonce']
valid_hash = review['sha256']
assert image.stat().st_size == 64 * 1024 * 1024
assert hashlib.sha256(image.read_bytes()).hexdigest() == valid_hash
bad_hash = hashlib.sha256((base / 'rescue-emulator/corrupt.ext3').read_bytes()).hexdigest()
nonce = secrets.token_hex(16)
gdbus = '/nix/store/3y6mm9jfr26nb3442565ir78qlyp3n9i-glib-2.88.3-bin/bin/gdbus'
assert Path(gdbus).is_file()
deadline = time.monotonic() + 240
while time.monotonic() < deadline:
    found = None
    for block in Path('/sys/class/block').glob('sd?'):
        usb = next((p for p in block.resolve().parents if (p / 'idVendor').exists()), None)
        if not usb or (usb / 'idVendor').read_text().strip() != '0525':
            continue
        if (usb / 'idProduct').read_text().strip() != 'a4a5':
            continue
        if int((block / 'size').read_text()) * 512 != 3269459968:
            continue
        found = '/dev/' + block.name
        break
    if found:
        break
    time.sleep(.25)
else:
    raise SystemExit('No distinct rescue USB; no host files written.')

print('Distinct bootstrap rescue USB detected', flush=True)
info = subprocess.run(['udisksctl', 'info', '-b', found], capture_output=True, text=True, check=True)
drive = re.search(r"Drive:\s+'([^']+)'", info.stdout)
assert drive and re.search(r'HintSystem:\s+false', info.stdout)
part = found + '1'
window = time.monotonic() + 40

def remaining():
    if time.monotonic() >= window:
        raise TimeoutError('Rescue write budget reached; unmount before reset.')

def digest_file(path):
    result = hashlib.sha256()
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd, 'rb') as stream:
        assert stat.S_ISREG(os.fstat(stream.fileno()).st_mode)
        while True:
            remaining()
            chunk = stream.read(1024 * 1024)
            if not chunk:
                return result.hexdigest()
            result.update(chunk)

mounted = False
proof = None
try:
    while True:
        remaining()
        result = subprocess.run(['findmnt', '-J', '-S', part, '-o', 'TARGET,FSTYPE,SOURCE'], capture_output=True, text=True)
        if result.returncode == 0:
            fs = json.loads(result.stdout)['filesystems']
            assert len(fs) == 1 and fs[0]['fstype'] == 'vfat' and fs[0]['source'] == part
            mount = Path(fs[0]['target'])
            mounted = True
            break
        result = subprocess.run(['udisksctl', 'mount', '-b', part], capture_output=True, text=True)
        time.sleep(.25)
    control = mount / 'gleworks-alpine'
    assert control.is_dir() and not control.is_symlink()
    target = control / 'system.ext3'
    assert target.stat().st_size == image.stat().st_size
    assert digest_file(target) == bad_hash
    partial = control / 'system.rescue-repair.partial'
    fd = os.open(partial, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, 'wb') as dst, image.open('rb') as src:
        while chunk := src.read(1024 * 1024):
            remaining()
            dst.write(chunk)
        dst.flush()
        os.fsync(dst.fileno())
    assert digest_file(partial) == valid_hash
    os.replace(partial, target)
    payload = f'GLEWORKS_INDEPENDENT_USB_RESCUE nonce={nonce} image_sha256={valid_hash}\n'.encode()
    name = 'rescue-proof-' + nonce + '.txt'
    fd = os.open(control / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, 'wb') as dst:
        dst.write(payload)
        dst.flush()
        os.fsync(dst.fileno())
    fd = os.open(control, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)
    remaining()
    assert (control / name).read_bytes() == payload
    proof = {'scope': 'Independent bootstrap USB with deliberately invalid Alpine image; host repair and nonce before reset',
             'usb_vid_pid': '0525:a4a5', 'nonce': nonce, 'proof_filename': name,
             'corrupt_image_sha256': bad_hash, 'repaired_image_sha256': valid_hash,
             'image_bytes': image.stat().st_size, 'safe_host_eject': False,
             'stock_return_roundtrip_verified': False, 'amazon_os_removed': False}
    print('Known invalid image repaired; nonce written and verified', flush=True)
finally:
    if subprocess.run(['findmnt', '-rn', '-S', part], capture_output=True).returncode == 0:
        subprocess.run(['udisksctl', 'unmount', '-b', part], capture_output=True, text=True, check=True)
    assert subprocess.run(['findmnt', '-rn', '-S', part], capture_output=True).returncode == 1
    subprocess.run([gdbus, 'call', '--system', '--dest', 'org.freedesktop.UDisks2', '--object-path', drive[1],
                    '--method', 'org.freedesktop.UDisks2.Drive.Eject', '{}'], capture_output=True, text=True, check=True)
    print('Rescue volume safely unmounted/ejected; cable retained', flush=True)
    if proof:
        proof['safe_host_eject'] = True
        (base / 'physical-usb-rescue-host-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
