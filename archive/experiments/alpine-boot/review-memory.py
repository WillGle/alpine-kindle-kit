#!/usr/bin/env python3
"""Derive the built ARM decompressor envelope; not a hardware/DMA approval."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess

base = Path(__file__).resolve().parent
nm = '/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin/llvm-nm'


def derive(directory):
    build = directory / 'build'
    image = (build / 'arch/arm/boot/zImage').read_bytes()
    raw = (build / 'arch/arm/boot/Image').read_bytes()
    dtb = (base.parent / 'native-boot/imx6sl-pw3-probe.dtb').read_bytes()
    lines = subprocess.check_output([nm, '-n', str(build / 'arch/arm/boot/compressed/vmlinux')], text=True)
    symbols = {a[2]: int(a[0], 16) for line in lines.splitlines() if len(a := line.split()) == 3}
    config = (build / '.config').read_text()
    assert '# CONFIG_ARM_ATAG_DTB_COMPAT is not set\n' in config
    assert '\nCONFIG_ZBOOT_ROM=y\n' not in config
    assert symbols['_edata'] == len(image)
    lc = symbols['LC1']
    stack_offset, edata_offset = struct.unpack_from('<II', image, lc)
    assert lc + edata_offset == len(image)
    entry = 0x80008000
    dtb_rounded = (len(dtb) + 7) & ~7
    bss_adjustment = max(0, symbols['_kernel_bss_size'] - (symbols['_edata'] - symbols['wont_overwrite']))
    inflated = len(raw) + bss_adjustment
    # head.S may tag r4 with bit0 while cache setup is deferred. Both paths
    # must yield the same aligned relocation for this fixed image.
    destinations = [((entry + tag + inflated + ((symbols['reloc_code_end'] - symbols['restart'] + 256) & ~255)) & ~255) for tag in (0, 1)]
    assert destinations[0] == destinations[1]
    target = destinations[0]
    source = entry + (symbols['restart'] & ~31)
    delta = target - source
    copy_size = ((entry + symbols['_edata'] + dtb_rounded - source) + 31) & ~31
    stack = entry + lc + stack_offset + dtb_rounded + delta
    heap_end = stack + 65536
    assert entry + inflated > entry + symbols['wont_overwrite']
    assert target > entry + inflated
    assert target + copy_size < stack < heap_end < 0xa0000000
    return {'image_sha256': hashlib.sha256(image + dtb).hexdigest(),
            'compressed_bytes': len(image), 'inflated_image_bytes': len(raw),
            'bss_size_adjustment': bss_adjustment, 'image_entry': entry,
            'inflated_end_exclusive': entry + len(raw),
            'copied_code_start': target, 'copied_code_end_exclusive': target + copy_size,
            'relocation_delta': delta, 'relocated_dtb_start': entry + len(image) + delta,
            'relocated_stack_top': stack, 'heap_end_exclusive': heap_end,
            'dtb_rounded_bytes': dtb_rounded}


# Check the derivation against the previously reviewed, preserved diagnostic.
old = derive(base.parent / 'native-boot')
record = json.loads((base.parent / 'native-handoff/memory-review.json').read_text())['derived_decompressor_layout']
for field in ['copied_code_start', 'copied_code_end_exclusive', 'relocation_delta',
              'relocated_dtb_start', 'relocated_stack_top', 'heap_end_exclusive']:
    assert old[field] == int(record[field], 16), field
report = derive(base)
for field in list(report):
    if field.endswith(('_start', '_exclusive', '_top', '_entry', '_delta')):
        report[field] = hex(report[field])
report.update({'method': 'Linux6.18 head.S with actual linked symbols and LC1 words; no target CPU execution',
               'prior_diagnostic_derivation_reproduced': True,
               'within_declared_512MiB_RAM': True,
               'hardware_reserved_memory_and_DMA_qualified': False,
               'native_boot_confirmed': False})
(base / 'memory-review.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
