#!/usr/bin/env python3
"""Validate existing boot-test inputs; never issue device commands."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import zlib

base = Path(__file__).resolve().parent
linux = base.parent
native = linux / 'native-boot'
bundle = base / 'bundle'
llvm = '/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin/'
results = []


def need(condition, detail):
    if not condition:
        raise ValueError(detail)


def check(name, action):
    try:
        action()
        results.append({'id': name, 'status': 'PASS'})
    except Exception as error:
        results.append({'id': name, 'status': 'FAIL', 'detail': str(error)})


def source():
    repo = linux / 'linux-6.18'
    need(subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
         == '7d0a66e4bb9081d75c82ec4957c50034cb0ea449', 'source revision changed')
    need(not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain', '--untracked-files=no']), 'tracked source changed')


def manifests():
    listed = {}
    for line in (bundle / 'gleworks-boot-gate.sha256').read_text().splitlines():
        expected, name = line.split()
        need(name not in listed, 'duplicate artifact '+name)
        listed[name] = expected
        need(Path(name).name == name, 'invalid artifact basename')
        need(hashlib.sha256((bundle / name).read_bytes()).hexdigest() == expected, name)
    artifacts = json.loads((base / 'artifacts.json').read_text())
    expected_names = {'gleworks_kexec_load.ko', 'gleworks-load-helper', 'gleworks-native-image.bin',
                      'gleworks-watchdog-read', 'gleworks-boot-readiness.sh',
                      'gleworks_watchdog_guard.ko', 'gleworks-watchdog-helper',
                      'gleworks-watchdog-trial.sh', 'gleworks-boot-gate-batch.sh'}
    need(set(listed) == set(artifacts) == expected_names, 'manifest/JSON membership mismatch')
    for name, meta in artifacts.items():
        data = (bundle / name).read_bytes()
        need(len(data) == meta['bytes'] and hashlib.sha256(data).hexdigest() == meta['sha256'], name)


def elf(data):
    need(data[:6] == b'\x7fELF\x01\x01' and struct.unpack_from('<H', data, 18)[0] == 40, 'not ARM ELF32 little endian')
    return data


def static_programs():
    for path in [native / 'init'] + [bundle / name for name in
            ['gleworks-load-helper', 'gleworks-watchdog-helper', 'gleworks-watchdog-read']]:
        data = elf(path.read_bytes())
        off = struct.unpack_from('<I', data, 28)[0]
        stride, count = struct.unpack_from('<HH', data, 42)
        need(all(struct.unpack_from('<I', data, off+i*stride)[0] not in [2, 3] for i in range(count)), path.name+' dynamic ELF')


def initramfs():
    data = (native / 'build/usr/initramfs_data.cpio').read_bytes()
    compressed = (native / 'build/arch/arm/boot/compressed/piggy_data').read_bytes()
    need(compressed in (native / 'build/arch/arm/boot/zImage').read_bytes(), 'compressed kernel blob not in zImage')
    raw = zlib.decompress(compressed, 31)
    need(raw == (native / 'build/arch/arm/boot/Image').read_bytes(), 'decompressed kernel differs from Image')
    need(data in raw, 'generated CPIO not embedded in actual kernel')
    offset, files = 0, {}
    while data[offset:offset+6] == b'070701':
        fields = [int(data[offset+6+i*8:offset+14+i*8], 16) for i in range(13)]
        size, length = fields[6], fields[11]
        name = data[offset+110:offset+110+length-1].decode()
        start = (offset+110+length+3) & ~3
        files[name] = (fields, data[start:start+size])
        offset = (start+size+3) & ~3
        if name == 'TRAILER!!!':
            break
    need(files['init'][1] == (native / 'init').read_bytes(), 'embedded PID1 differs')
    need(files['init'][0][1:4] == [0o100755, 0, 0], 'PID1 ownership/mode')
    need(files['dev/console'][0][9:11] == [5, 1], 'console device')
    need(all(name in files for name in ['dev', 'proc', 'sys']), 'missing mountpoints')


def image():
    kernel = (native / 'build/arch/arm/boot/zImage').read_bytes()
    dtb = (native / 'imx6sl-pw3-probe.dtb').read_bytes()
    need(struct.unpack_from('<I', kernel, 36)[0] == 0x016f2818, 'zImage magic')
    start, end = struct.unpack_from('<II', kernel, 40)
    need(end-start == len(kernel), 'zImage length')
    need(struct.unpack_from('>II', dtb) == (0xd00dfeed, len(dtb)), 'DTB envelope')
    need((bundle / 'gleworks-native-image.bin').read_bytes() == kernel+dtb, 'appended image differs')
    need(1048576 < len(kernel+dtb) < 2097152, 'image outside loader limits')


def board():
    data = (native / 'imx6sl-pw3-probe.dtb').read_bytes()
    header = struct.unpack_from('>10I', data)
    offset, strings = header[2], header[3]
    stack, nodes = [], {}
    while True:
        token = struct.unpack_from('>I', data, offset)[0]
        offset += 4
        if token == 1:
            end = data.index(0, offset)
            stack.append(data[offset:end].decode())
            offset = (end+4) & ~3
            nodes['/'.join(stack)] = {}
        elif token == 2:
            stack.pop()
        elif token == 3:
            size, nameoff = struct.unpack_from('>II', data, offset)
            offset += 8
            nameend = data.index(0, strings+nameoff)
            name = data[strings+nameoff:nameend].decode()
            nodes['/'.join(stack)][name] = data[offset:offset+size]
            offset = (offset+size+3) & ~3
        elif token == 9:
            break
        else:
            need(token == 4, 'invalid DTB token')
    need(nodes['/memory@80000000']['reg'] == struct.pack('>II', 0x80000000, 0x20000000), 'board memory')
    usb = [props for path, props in nodes.items() if path.endswith('/usb@2184000')]
    need(len(usb) == 1 and usb[0].get('status') == b'okay\0' and usb[0].get('dr_mode') == b'peripheral\0', 'USB peripheral DTB')


def config():
    text = (native / 'build/.config').read_text()
    for name in ['MMC', 'MTD', 'DRM', 'FB', 'NET', 'PM', 'CPU_IDLE', 'CPU_FREQ', 'WATCHDOG', 'ARM_ATAG_DTB_COMPAT']:
        need('# CONFIG_'+name+' is not set\n' in text, name+' unexpectedly enabled')
    for name in ['SOC_IMX6SL', 'ARM_APPENDED_DTB', 'CMDLINE_FORCE', 'USB_G_SERIAL', 'USB_CHIPIDEA_IMX', 'USB_MXS_PHY', 'BLK_DEV_INITRD']:
        need('CONFIG_'+name+'=y\n' in text, name+' missing')
    need('CONFIG_CMDLINE="rdinit=/init clk_ignore_unused regulator_ignore_unused panic=0"' in text, 'command line changed')


def readback():
    image = (bundle / 'gleworks-native-image.bin').read_bytes()
    expected = json.loads((native / 'host-review.json').read_text())['expected_padded_fnv1a']
    for size in [4096, 65536, 262144, 1048576, len(image)]:
        padded = image[:size]+b'\0'*((-size) % 4096)
        value = 2166136261
        for byte in padded:
            value = ((value ^ byte)*16777619) & 0xffffffff
        need(f'{value:08x}' == expected[str(size)], 'FNV mismatch at '+str(size))


def modules():
    symbols = (linux.parent / 'docs/evidence/private/captures/gleworks-module-capture-20261029-194813-17967/kallsyms.txt').read_text()
    for name in ['gleworks_kexec_load.ko', 'gleworks_watchdog_guard.ko']:
        data = elf((bundle / name).read_bytes())
        need(b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 p2v8 ' in data, name+' vermagic')
        off = struct.unpack_from('<I', data, 32)[0]
        stride, count, names = struct.unpack_from('<HHH', data, 46)
        sections = [struct.unpack_from('<10I', data, off+i*stride) for i in range(count)]
        table = sections[names]
        namesdata = data[table[4]:table[4]+table[5]]
        named = {namesdata[s[0]:namesdata.index(0, s[0])].decode(): s for s in sections}
        need(named['.gnu.linkonce.this_module'][5] == 292, name+' descriptor size')
        rel = named['.rel.gnu.linkonce.this_module']
        need([struct.unpack_from('<I', data, i)[0] for i in range(rel[4], rel[4]+rel[5], 8)] == [188, 284], name+' init/exit offsets')
        imports = subprocess.check_output([llvm+'llvm-nm', '-u', str(bundle / name)], text=True)
        for line in imports.splitlines():
            symbol = line.split()[-1]
            need(' __ksymtab_'+symbol+'\n' in symbols, 'unexported import '+symbol)


def stock_images():
    for name in ['stock-main', 'stock-diags']:
        data = (linux.parent / 'docs/evidence/private/captures/boot-preparation' / (name+'.uImage')).read_bytes()
        magic, crc, timestamp, size = struct.unpack_from('>4I', data)
        need(magic == 0x27051956, name+' header magic')
        need(len(data) >= 64+size, name+' truncated declared payload')
        header = data[:4]+b'\0'*4+data[8:64]
        need(zlib.crc32(header) == crc, name+' header CRC')
        need(zlib.crc32(data[64:64+size]) == struct.unpack_from('>I', data, 24)[0], name+' payload CRC')
        need(struct.unpack_from('>II', data, 16) == (0x80008000, 0x80008000), name+' entry/load')


for name, action in [('H01-source', source), ('H01-bundle', manifests), ('H02-static-ARM', static_programs),
                     ('H03-initramfs', initramfs), ('H04-image', image), ('H05-H07-DTB', board),
                     ('H06-H07-config', config), ('H08-readback', readback), ('H09-module-ABI', modules),
                     ('H10-stock-images', stock_images)]:
    check(name, action)
report = {'scope': 'Host structural validation only; no device execution.', 'checks': results,
          'provenance_limit': 'Source revision/cleanliness and generated config are inspected separately; not proof of build provenance. Actual CPIO bytes are checked inside the decompressed bundled kernel.',
          'boot_decision': 'NO-GO', 'blocking_gates': ['R01: native handoff unimplemented',
          'D10: watchdog timeout/reset/stock return unproven'],
          'untested_gates': ['Physical PW3 USB/clock/power under kernel6.18', 'P01 live native PID1',
                            'P02 native-to-stock return', 'P03 Alpine runtime', 'P04 product acceptance']}
(base / 'host-audit.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
raise SystemExit(any(item['status'] == 'FAIL' for item in results))
