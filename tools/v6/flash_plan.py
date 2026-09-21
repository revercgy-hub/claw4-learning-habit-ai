"""Validate an M0 flash plan against the saved device layout. Does not flash."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

FLASH_SIZE = 32 * 1024 * 1024
TABLE_OFFSET = 0x9000


def partitions(data):
    result = []
    for offset in range(0, len(data), 32):
        entry = data[offset:offset + 32]
        if len(entry) != 32:
            raise ValueError('Truncated partition table')
        if entry[:2] == b'\xeb\xeb':
            if entry[16:] != hashlib.md5(data[:offset]).digest():
                raise ValueError('Partition table MD5 mismatch')
            return result
        magic, kind, subtype, start, size, label, flags = struct.unpack('<HBBII16sI', entry)
        if magic != 0x50aa:
            raise ValueError('Invalid or unsigned partition table')
        if not size or start + size > FLASH_SIZE:
            raise ValueError('Partition exceeds flash')
        name = label.split(b'\0', 1)[0].decode('ascii')
        result.append((name, kind, subtype, start, size, flags))
    raise ValueError('Missing partition MD5')


def validate_layout(before, after):
    expected = [('assets' if p[0] == 'resources' else p[0], *p[1:]) for p in before]
    if after != expected:
        raise ValueError('Candidate moves/resizes/retypes an existing partition')
    by_name = {p[0]: p for p in after}
    if len(by_name) != len(after):
        raise ValueError('Duplicate partition labels')
    for name, start, size in [('factory', 0x200000, 14 * 1024 * 1024),
                              ('assets', 0x1000000, 15 * 1024 * 1024)]:
        if name not in by_name or by_name[name][3:5] != (start, size):
            raise ValueError(f'Unexpected {name} region')
    ranges = sorted((p[3], p[3] + p[4]) for p in after)
    if any(ranges[i][1] > ranges[i + 1][0] for i in range(len(ranges) - 1)):
        raise ValueError('Overlapping partition layout')


def validate_writes(writes):
    allowed = {0x2000: TABLE_OFFSET, TABLE_OFFSET: 0xa000, 0x10f000: 0x200000,
               0x200000: 0x1000000, 0x1000000: 0x1f00000}
    if {start for start, size in writes} != set(allowed) or len(writes) != len(allowed):
        raise ValueError('Expected exactly bootloader, partition, speech models, factory app and assets')
    for start, size in writes:
        if size <= 0 or start + size > allowed[start]:
            raise ValueError('Image crosses an allowed region (NVS must be preserved)')


def make_plan(backup, build):
    saved = backup.read_bytes()
    if len(saved) != FLASH_SIZE:
        raise ValueError('A complete 32MiB device backup is required')
    args = json.loads((build / 'flasher_args.json').read_text())
    entries = []
    for offset, relative in args['flash_files'].items():
        path = (build / relative).resolve()
        if not path.is_relative_to(build.resolve()):
            raise ValueError('Flash image escapes build directory')
        image = path.read_bytes()
        entries.append({'offset': int(offset, 0), 'path': str(path), 'size': len(image),
                        'sha256': hashlib.sha256(image).hexdigest()})
    validate_writes([(entry['offset'], entry['size']) for entry in entries])
    table = next(entry for entry in entries if entry['offset'] == TABLE_OFFSET)
    validate_layout(partitions(saved[TABLE_OFFSET:TABLE_OFFSET + 4096]),
                    partitions(Path(table['path']).read_bytes()))
    return {'status': 'PLAN_ONLY_NOT_FLASHED', 'backup_path': str(backup.resolve()),
            'backup_sha256': hashlib.sha256(saved).hexdigest(),
            'preserve': ['nvsfactory', 'nvs', 'phy_init', 'factory_test'],
            'writes': sorted(entries, key=lambda e: e['offset'])}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backup', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    plan = make_plan(args.backup, args.build)
    args.output.write_text(json.dumps(plan, indent=2), encoding='utf-8')
    print('Flash plan validated; no device operation performed.')
