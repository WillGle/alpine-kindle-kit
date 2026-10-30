#!/usr/bin/env python3
"""Inspect compile-only handoff artifacts; never load or execute ARM code."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess

base = Path(__file__).resolve().parent
llvm = '/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin/'
blob = (base / 'relocate.bin').read_bytes()
assert len(blob) == 180 and len(blob) <= 4096
data = (base / 'relocate.o').read_bytes()
assert data[:6] == b'\x7fELF\x01\x01' and struct.unpack_from('<H', data, 18)[0] == 40
offset = struct.unpack_from('<I', data, 32)[0]
stride, count = struct.unpack_from('<HH', data, 46)
sections = [struct.unpack_from('<10I', data, offset+i*stride) for i in range(count)]
assert all(section[1] not in [4, 9] or not section[5] for section in sections), 'blob has linker relocations'
symbols = {}
for line in subprocess.check_output([llvm+'llvm-nm', '-n', str(base/'relocate.o')], text=True).splitlines():
    address, kind, name = line.split()
    symbols[name] = int(address, 16)
assert symbols == {'gw_relocate_start': 0, 'gw_relocate_list': 168,
                   'gw_relocate_entry': 172, 'gw_relocate_dtb': 176, 'gw_relocate_end': 180}
assert blob[168:] == b'\0'*12
assert struct.unpack_from('<7I', blob) == (0xf57ff04f, 0xee110f10, 0xe3c00001,
                                         0xe3c00004, 0xe3c00a01, 0xee010f10, 0xf57ff06f)
assert struct.unpack_from('<4I', blob, 152) == (0xe3a00000, 0xe3e01000, 0xe59f2008, 0xe1a0f00e)
assert not list(base.glob('*.ko')), 'compile-only area contains a loadable module'
imports = [line.split()[-1] for line in subprocess.check_output(
    [llvm+'llvm-nm', '-u', str(base/'transition.o')], text=True).splitlines()]
captured = (base.parent.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/kallsyms.txt').read_text()
ordinary = [name for name in imports if not name.startswith('gw_relocate_')]
assert all(' __ksymtab_'+name+'\n' in captured for name in ordinary)
review = {
    'scope': 'Compile-only objects/blob, no device access, no loadable transition module',
    'relocation_blob_bytes': len(blob), 'linker_relocations': 0,
    'parameter_offsets': symbols, 'ordinary_imports_with_captured_exports': ordinary,
    'mmu_and_id_cache_clear_prefix_verified': True, 'boot_register_suffix_verified': True,
    'artifacts': {name: {'bytes': (base/name).stat().st_size,
                        'sha256': hashlib.sha256((base/name).read_bytes()).hexdigest()}
                  for name in ['transition.o', 'chain.o', 'relocate.o', 'relocate.bin']},
    'native_boot_attempted': False,
    'unresolved': ['Wire chain validator to allocator-owned registry and fixed staged image at integration',
                   'Runtime callback identity and outer-cache function validation',
                   'Device/DMA shutdown effects, especially USB/MMC/EPDC',
                   'Watchdog reset cause and reset reliability after device shutdown/new kernel',
                   'Actual minimal image staging and physical native USB response'],
}
(base/'host-review.json').write_text(json.dumps(review, indent=2)+'\n')
print('ARM blob layout/MMU prefix/boot registers/export imports verified; compile-only, no boot performed.')
