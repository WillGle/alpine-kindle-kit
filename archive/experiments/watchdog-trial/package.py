#!/usr/bin/env python3
"""Package fixed watchdog trial artifacts; host only."""
from pathlib import Path
import hashlib
import json
import subprocess

base = Path(__file__).resolve().parent
llvm = '/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin/'
symbols = (base.parent.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/kallsyms.txt').read_text()
imports = subprocess.check_output([llvm+'llvm-nm', '-u', str(base/'gleworks_watchdog_guard.ko')], text=True)
names = [line.split()[-1] for line in imports.splitlines()]
for name in names:
    assert ' __ksymtab_'+name+'\n' in symbols, name
assert 'c04542c8 T imx2_wdt_reinit\n' in symbols
data = (base/'gleworks_watchdog_guard.ko').read_bytes()
assert b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 p2v8 ' in data
assert not subprocess.check_output([llvm+'llvm-nm', '-u', str(base/'gleworks-watchdog-helper')], text=True).strip()
artifacts = {
    'gleworks_watchdog_guard.ko': data,
    'gleworks-watchdog-helper': (base/'gleworks-watchdog-helper').read_bytes(),
    'gleworks-watchdog-read': (base.parent/'native-boot/gleworks-watchdog-read').read_bytes(),
}
hashes = {name: hashlib.sha256(data).hexdigest() for name, data in artifacts.items()}
script = (base/'trial.in.sh').read_text()
for token, name in [('MODULE','gleworks_watchdog_guard.ko'),('HELPER','gleworks-watchdog-helper'),('READER','gleworks-watchdog-read')]:
    script = script.replace('@'+token+'_HASH@', hashes[name])
assert '@' not in script
artifacts['gleworks-watchdog-trial.sh'] = script.encode()
bundle = base/'bundle'
bundle.mkdir(exist_ok=True)
for name,data in artifacts.items():
    (bundle/name).write_bytes(data)
(bundle/'gleworks-watchdog-trial.sha256').write_text(''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in artifacts.items()))
(base/'host-review.json').write_text(json.dumps({
    'imports_checked_against_captured_exports': names,
    'reinit_is_nonexported_runtime_resolved': True,
    'reinit_exact_captured_address': '0xc04542c8',
    'restore_deadline_seconds': 150,
    'max_arms_per_load': 2,
    'changes_watchdog_timeout': False,
    'boot_attempted': False,
    'watchdog_reset_proven': False,
    'artifacts': {name:{'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()} for name,data in artifacts.items()},
},indent=2)+'\n')
print('Host watchdog module/helper built; captured exports match. Hardware untested.')
