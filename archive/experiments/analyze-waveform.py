#!/usr/bin/env python3
"""Read captured PW3 evidence and report structure; never produce firmware."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


def analyze(folder):
    folder = folder.resolve()
    for line in (folder / 'SHA256SUMS').read_text().splitlines():
        digest, name = line.split(None, 1)
        path = (folder / name.strip()).resolve()
        if not path.is_relative_to(folder):
            raise ValueError('Manifest path outside capture')
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError('Capture hash mismatch')
    active = (folder / 'active-waveform.bin').read_bytes()
    panel = (folder / 'panel-waveform.bin').read_bytes()
    for name, data in [('active-waveform.bin', active), ('panel-waveform.bin', panel)]:
        if data != (folder / (name + '.second')).read_bytes():
            raise ValueError('Duplicate reads differ')
    if len(panel) < 48 or active[:63] != panel[:63]:
        raise ValueError('Unexpected or differing headers')
    checksum, length = struct.unpack_from('<II', panel)
    if not 48 <= length <= len(panel):
        raise ValueError('Invalid declared panel length')
    if zlib.crc32(bytes(4) + panel[4:length]) != checksum:
        raise ValueError('Panel CRC mismatch')
    modes, temps = active[37] + 1, active[38] + 1
    offset = 48 + temps + 1
    payload = active[offset:]
    if len(payload) < modes * 8:
        raise ValueError('Truncated mode table')
    tables = []
    for mode in range(modes):
        table = struct.unpack_from('<Q', payload, mode * 8)[0]
        if table < modes * 8 or table + temps * 8 > len(payload):
            raise ValueError('Temperature table outside payload')
        pointers = struct.unpack_from('<' + 'Q' * temps, payload, table)
        if any(p < table + temps * 8 or p >= len(payload) or p % 8 for p in pointers):
            raise ValueError('Invalid waveform pointer')
        tables.append({'mode': mode, 'temperature_table': table,
                       'first_data': min(pointers), 'last_data': max(pointers)})
    voltage = {'advanced_flags': active[39], 'record_checksums_valid': False}
    if active[39] in (1, 3):
        begin, end = struct.unpack_from('<QQ', payload, modes * 8)
        if not 0 <= begin <= end <= len(payload) or end - begin != modes * temps * 16:
            raise ValueError('Unexpected voltage table extent')
        rows = [payload[i:i + 16] for i in range(begin, end, 16)]
        valid = sum((sum(row[:15]) & 255) == row[15] for row in rows)
        voltage.update(records=len(rows), valid_checksums=valid,
                       unique_control_records=len({row[:14] for row in rows}),
                       record_checksums_valid=valid == len(rows))
        if valid != len(rows):
            raise ValueError('Voltage record checksum mismatch')
    return {'status': 'STRUCTURE_CHECKED_NOT_BOOT_APPROVED',
            'active_bytes': len(active), 'panel_bytes': len(panel),
            'panel_declared_bytes': length, 'panel_crc_valid': True,
            'panel_tail_zero': not any(panel[length:]),
            'active_declared_crc_valid': zlib.crc32(bytes(4) + active[4:length]) == checksum,
            'five_bit': active[36] & 12 == 4, 'mode_version': active[16],
            'mode_count': modes, 'temperature_entries': temps,
            'payload_offset': offset, 'mode_tables': tables, 'voltage': voltage,
            'limits': 'Payload encoding, algorithm data, PMIC use and display operation unvalidated.'}


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: analyze-waveform.py CAPTURE_DIRECTORY')
    print(json.dumps(analyze(Path(sys.argv[1])), indent=2))
