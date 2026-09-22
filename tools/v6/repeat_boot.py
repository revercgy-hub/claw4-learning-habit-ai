"""Bounded reset validation; stops on the first failed or ambiguous boot."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import time
from pathlib import Path


def classify(text, expected_elf):
    text = re.sub(r'\x1b\[[0-9;]*m', '', text)
    hashes = re.findall(r'CANDIDATE_ELF_SHA256=([a-f0-9]+)', text)
    blocked = any(token in text for token in ('BOOT_BLOCKED', 'abort() was called',
                                              'assert failed', 'Guru Meditation'))
    attempts = text.count('Calling app_main()')
    ready = text.count('V6M0: BOOT_READY')
    return {'passed': not blocked and attempts == 1 and ready == 1 and
            len(hashes) == 1 and expected_elf.startswith(hashes[0]),
            'attempts': attempts, 'boot_ready': ready, 'blocked_or_panic': blocked,
            'i2c_retries': text.count('I2C_RETRY'),
            'i2c_recovered': text.count('I2C_RECOVERED')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=20)
    args = parser.parse_args()
    if not 1 <= args.rounds <= 20:
        parser.error('rounds must be 1..20')
    args.output_dir.mkdir(parents=True, exist_ok=False)
    candidate = json.loads(args.candidate.read_text(encoding='utf-8'))
    expected = candidate['files']['build/xiaozhi.elf']['sha256']
    result = {'candidate': args.candidate.name, 'elf_sha256': expected,
              'requested_rounds': args.rounds, 'capture_seconds_per_round': 20,
              'rounds': [], 'status': 'IN_PROGRESS'}
    for number in range(1, args.rounds + 1):
        log = args.output_dir / f'boot-{number:02}.txt'
        start = time.monotonic()
        command = [sys.executable, str(Path(__file__).with_name('capture_device.py')),
                   '--port', args.port, '--seconds', '20', '--reset', '--output', str(log)]
        capture = subprocess.run(command, stdout=subprocess.DEVNULL)
        raw = log.read_bytes() if log.exists() else b''
        row = classify(raw.decode('utf-8', errors='replace'), expected)
        row.update(round=number, sha256=hashlib.sha256(raw).hexdigest(),
                   elapsed_seconds=round(time.monotonic() - start, 2),
                   capture_exit=capture.returncode)
        row['passed'] = row['passed'] and capture.returncode == 0
        result['rounds'].append(row)
        result['status'] = 'IN_PROGRESS' if row['passed'] else 'FAILED_STOPPED'
        if row['passed'] and number == args.rounds:
            result['status'] = 'PASS_RESET_SERIES_ONLY'
        (args.output_dir / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(json.dumps(row), flush=True)
        if not row['passed']:
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
