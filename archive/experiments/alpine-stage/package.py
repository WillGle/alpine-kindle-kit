#!/usr/bin/env python3
"""Package a returning preparation trial; no device access or execution path."""
from pathlib import Path
import hashlib
import json
import subprocess

base = Path(__file__).resolve().parent
root = base.parent
inputs = {
    'gleworks_alpine_stage.ko': base/'gleworks_alpine_stage.ko',
    'gleworks-alpine-stage-helper': base/'gleworks-alpine-stage-helper',
    'gleworks-alpine-image.bin': root/'alpine-boot/alpine-zImage-dtb.bin',
    'gleworks-watchdog-read': root/'native-boot/gleworks-watchdog-read',
}
image = inputs['gleworks-alpine-image.bin'].read_bytes()
assert len(image) == 2722513
assert hashlib.sha256(image).hexdigest() == '16c9be187c4ddefe61b2b5e184b03085db557f19acc9a9191c036cfa947ab92f'
llvm = '/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin/'
module = inputs['gleworks_alpine_stage.ko']
symbol_lines = subprocess.check_output([llvm+'llvm-nm', str(module)], text=True).splitlines()
symbols = {line.split()[-1] for line in symbol_lines}
assert {'gw_prepare_handoff', 'gw_validate_chain', 'gw_relocate_start'} <= symbols
assert not ({'gw_enter_handoff', 'machine_kexec', 'kernel_restart', 'kernel_restart_prepare', 'machine_restart', 'soft_restart'} & symbols)
captured = (root.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/kallsyms.txt').read_text()
imports = [line.split()[-1] for line in symbol_lines if line.strip().startswith('U ')]
assert all(' __ksymtab_'+name+'\n' in captured for name in imports)
for path in [module, inputs['gleworks-alpine-stage-helper']]:
    data = path.read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01' and int.from_bytes(data[18:20], 'little') == 40
assert not subprocess.check_output([llvm+'llvm-nm', '-u', str(inputs['gleworks-alpine-stage-helper'])], text=True).strip()
log = (base/'build.log').read_text()
assert 'warning:' not in log and 'error:' not in log
hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name,path in inputs.items()}
script = (base/'trial.in.sh').read_text()
for token, name in [('MODULE', 'gleworks_alpine_stage.ko'), ('HELPER', 'gleworks-alpine-stage-helper'),
                    ('IMAGE', 'gleworks-alpine-image.bin'), ('READER', 'gleworks-watchdog-read')]:
    script = script.replace('@'+token+'_HASH@', hashes[name])
assert '@' not in script
subprocess.run(['sh', '-n'], input=script, text=True, check=True)
artifacts = {name: path.read_bytes() for name,path in inputs.items()}
artifacts['gleworks-alpine-stage.sh'] = script.encode()
bundle = base/'bundle'
bundle.mkdir(exist_ok=True)
for name,data in artifacts.items():
    (bundle/name).write_bytes(data)
manifest = ''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in artifacts.items())
(bundle/'SHA256SUMS').write_text(manifest)
review = {'scope': 'Returning allocator-owned chain, fixed SHA256 and control-page preparation only',
          'artifacts': {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()} for name,data in artifacts.items()},
          'ordinary_imports_match_captured_exports': imports,
          'build_warnings': 0, 'execute_interface': False,
          'device_tested': False, 'native_boot_confirmed': False,
          'runtime_gates': ['new module load/fault/prepare/reject/cleanup behavior',
                            'successful fixed Alpine-image staging with current free RAM'],
          'boot_gates': ['all peripheral DMA quiescence', 'watchdog recovery after shutdown/new kernel',
                         'decompressor workspace versus hardware reserved RAM', 'fresh native USB PID1 challenge']}
(base/'host-review.json').write_text(json.dumps(review, indent=2)+'\n')
print('Prepare-only ARM module/imports/helper/image/shell checked; 5-file bundle packaged. No device test or native boot.')
