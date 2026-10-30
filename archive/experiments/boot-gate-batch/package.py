#!/usr/bin/env python3
"""Consolidate existing reviewed inputs; no device access or compilation."""
from pathlib import Path
import hashlib
import json

base = Path(__file__).resolve().parent
inputs = {}
for child, manifest in [('native-boot/readiness-bundle', 'gleworks-native-readiness.sha256'),
                        ('watchdog-trial/bundle', 'gleworks-watchdog-trial.sha256')]:
    directory = base.parent / child
    for line in (directory / manifest).read_text().splitlines():
        expected, name = line.split()
        assert Path(name).name == name
        data = (directory / name).read_bytes()
        assert hashlib.sha256(data).hexdigest() == expected, name
        if name in inputs:
            assert inputs[name] == data, name
        inputs[name] = data
checks = []
for name, data in inputs.items():
    expected = hashlib.sha256(data).hexdigest()
    checks.append(f'[ -f "{name}" ] && [ ! -L "{name}" ] || exit 1')
    checks.append(f'[ "$(sha256sum "{name}" | awk \'{{print $1}}\')" = "{expected}" ] || exit 1')
launcher = (base / 'launcher.in.sh').read_text().replace('@HASH_CHECKS@', '\n'.join(checks))
assert '@HASH_CHECKS@' not in launcher
inputs['gleworks-boot-gate-batch.sh'] = launcher.encode()
bundle = base / 'bundle'
bundle.mkdir(exist_ok=True)
for name, data in inputs.items():
    (bundle / name).write_bytes(data)
(bundle / 'gleworks-boot-gate.sha256').write_text(''.join(
    hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name, data in inputs.items()))
(base / 'artifacts.json').write_text(json.dumps({name: {
    'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()
} for name, data in inputs.items()}, indent=2)+'\n')
print(f'{len(inputs)} inputs packaged; original child artifacts unchanged. No device command executed.')
