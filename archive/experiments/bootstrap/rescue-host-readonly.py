#!/usr/bin/env python3
"""Observe the distinct rescue gadget without mounting or writing storage."""
from pathlib import Path
import json
import subprocess
import time

base = Path(__file__).resolve().parent
deadline = time.monotonic() + 240
while time.monotonic() < deadline:
    for block in Path('/sys/class/block').glob('sd?'):
        usb = next((p for p in block.resolve().parents if (p / 'idVendor').exists()), None)
        if not usb or (usb / 'idVendor').read_text().strip() != '0525':
            continue
        if (usb / 'idProduct').read_text().strip() != 'a4a5':
            continue
        if int((block / 'size').read_text()) * 512 != 3269459968:
            continue
        node = '/dev/' + block.name
        # The automounter may have mounted it; unmount immediately before reset.
        mounts = subprocess.run(['findmnt', '-rn', '-S', node + '1'], capture_output=True, text=True, timeout=5)
        if mounts.returncode == 0:
            subprocess.run(['udisksctl', 'unmount', '-b', node + '1'], capture_output=True, timeout=10, check=True)
        assert subprocess.run(['findmnt', '-rn', '-S', node + '1'], capture_output=True, timeout=5).returncode == 1
        proof = {'scope': 'Distinct physical bootstrap rescue USB after intentionally invalid Alpine image; enumeration only',
                 'usb_vid_pid': '0525:a4a5', 'export_bytes': 3269459968,
                 'host_writes_performed': False, 'partition_unmounted': True,
                 'repair_verified': False, 'amazon_os_removed': False}
        (base / 'physical-usb-rescue-enumeration.json').write_text(json.dumps(proof, indent=2) + '\n')
        print('PASS distinct bootstrap rescue USB enumeration; no host writes, partition unmounted', flush=True)
        raise SystemExit(0)
    time.sleep(.25)
raise SystemExit('No distinct rescue USB observed; no host writes performed.')
