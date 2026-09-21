"""Bounded serial evidence capture. Raw logs stay in a private ignored directory."""
import argparse
import re
import time
from pathlib import Path
import serial


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--seconds', type=int, default=45)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reset', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.seconds <= 60:
        parser.error('Capture duration must be 1..60 seconds')
    if args.output.exists():
        parser.error('Refusing to overwrite existing evidence')
    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = 115200
    connection.timeout = 0.25
    connection.dtr = False
    connection.rts = False
    connection.open()
    try:
        if args.reset:
            from esptool.reset import HardReset
            HardReset(connection)()
        deadline = time.monotonic() + args.seconds
        with args.output.open('wb') as output:
            while time.monotonic() < deadline:
                chunk = connection.read(max(1, connection.in_waiting))
                output.write(chunk)
    finally:
        connection.close()
    text = args.output.read_bytes().decode('utf-8', errors='replace')
    text = re.sub(r'\x1b\[[0-9;]*m', '', text)
    markers = [line for line in text.splitlines()
               if any(token in line for token in ('V6M0:', 'Claw4V6:', 'Claw4Audio:',
                                                   'assert failed', 'Guru Meditation', 'abort()'))]
    print('\n'.join(markers[-40:]))
    print(f'Captured {args.output.stat().st_size} bytes; raw log retained locally.')
