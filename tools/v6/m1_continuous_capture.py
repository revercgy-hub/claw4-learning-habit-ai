"""Bounded continuous UART capture with private raw bytes and a timing index.

Keep the raw output in approved private storage. The JSON index and state file
contain timing and byte counts only; serial payloads are never printed or parsed.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from typing import Callable, Protocol


class SerialLike(Protocol):
    in_waiting: int

    def read(self, size: int) -> bytes: ...
    def close(self) -> None: ...


def utc_timestamp() -> str:
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00', 'Z')


def _write_state(path: Path, metadata: dict) -> None:
    with path.open('x', encoding='utf-8', newline='\n') as state:
        state.write(json.dumps(metadata, sort_keys=True) + '\n')


def capture_loop(
    connection: SerialLike,
    output_path: Path,
    index_path: Path,
    state_path: Path,
    *,
    port: str,
    max_seconds: float,
    stop_file: Path | None = None,
    post_stop_seconds: float = 2.0,
    reset: bool = False,
    monotonic: Callable[[], float] = time.monotonic,
    utc_now: Callable[[], str] = utc_timestamp,
) -> dict:
    """Capture until the duration or stop-file deadline, then close the port.

    The three destinations are created exclusively. `connection` is closed on
    all exits, including destination errors and serial/read/write failures.
    """
    try:
        try:
            paths = (output_path, index_path, state_path)
            if stop_file is not None and stop_file.resolve() in {p.resolve() for p in paths}:
                raise ValueError('Stop-file path must differ from capture artifacts')
            if len({p.resolve() for p in paths}) != len(paths):
                raise ValueError('Output, index, and state paths must be different')
            if any(path.exists() for path in paths):
                raise FileExistsError('Refusing to overwrite capture artifacts')
            if stop_file is not None and stop_file.exists():
                raise FileExistsError('Stop-file already exists; use a fresh marker path')
            if not 1 <= max_seconds <= 600:
                raise ValueError('max_seconds must be 1..600')
            if not 0 <= post_stop_seconds <= 30:
                raise ValueError('post_stop_seconds must be 0..30')
            started = monotonic()
            started_utc = utc_now()
            stop_seen_elapsed_ms: int | None = None
            stop_deadline: float | None = None
            bytes_captured = 0
            chunks = 0
            stop_reason = 'max_seconds'
            # Exclusive creation prevents silently replacing any evidence artifact.
            with output_path.open('xb') as raw, index_path.open('x', encoding='utf-8', newline='\n') as index:
                while True:
                    now = monotonic()
                    elapsed = now - started
                    if elapsed >= max_seconds:
                        stop_reason = 'max_seconds'
                        break
                    if stop_file is not None and stop_deadline is None and stop_file.exists():
                        stop_seen_elapsed_ms = max(0, int(elapsed * 1000))
                        stop_deadline = now + post_stop_seconds
                    if stop_deadline is not None and now >= stop_deadline:
                        stop_reason = 'stop_file'
                        break

                    available = max(0, int(connection.in_waiting))
                    payload = connection.read(max(1, min(available or 1, 4096)))
                    if not payload:
                        continue
                    raw.write(payload)
                    raw.flush()
                    bytes_captured += len(payload)
                    chunks += 1
                    elapsed_ms = max(0, int((monotonic() - started) * 1000))
                    index.write(json.dumps({
                        'utc': utc_now(),
                        'elapsed_ms': elapsed_ms,
                        'bytes': len(payload),
                        'end_offset': bytes_captured,
                    }, sort_keys=True) + '\n')
                    index.flush()

            ended = monotonic()
            result = {
                'schema': 'm1-continuous-capture-state-v1',
                'status': 'complete',
                'port': port,
                'baudrate': 115200,
                'started_utc': started_utc,
                'elapsed_ms': max(0, int((ended - started) * 1000)),
                'bytes_captured': bytes_captured,
                'chunks': chunks,
                'stop_reason': stop_reason,
                'stop_seen_elapsed_ms': stop_seen_elapsed_ms,
                'reset_requested': reset,
            }
            _write_state(state_path, result)
            return result
        except BaseException as exc:
            # Validation errors precede timer/artifact setup and need no state file.
            if 'started' in locals():
                try:
                    if not state_path.exists():
                        _write_state(state_path, {
                            'schema': 'm1-continuous-capture-state-v1',
                            'status': 'error',
                            'port': port,
                            'started_utc': started_utc,
                            'elapsed_ms': max(0, int((monotonic() - started) * 1000)),
                            'bytes_captured': bytes_captured,
                            'chunks': chunks,
                            'stop_reason': 'error',
                            'error_type': type(exc).__name__,
                            'reset_requested': reset,
                        })
                except OSError:
                    pass
            raise
    finally:
        connection.close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', required=True, type=Path, help='Private raw byte output')
    parser.add_argument('--index', required=True, type=Path, help='JSONL timing and byte-offset index')
    parser.add_argument('--state', required=True, type=Path, help='Payload-free capture metadata JSON')
    parser.add_argument('--max-seconds', required=True, type=float)
    parser.add_argument('--stop-file', type=Path)
    parser.add_argument('--post-stop-seconds', type=float, default=2.0)
    parser.add_argument('--reset', action='store_true', help='Request esptool USB reset after opening the serial port')
    args = parser.parse_args(argv)
    if not 1 <= args.max_seconds <= 600:
        parser.error('--max-seconds must be 1..600')
    if not 0 <= args.post_stop_seconds <= 30:
        parser.error('--post-stop-seconds must be 0..30')
    destinations = (args.output, args.index, args.state)
    if len({p.resolve() for p in destinations}) != len(destinations):
        parser.error('--output, --index, and --state must be different paths')
    if args.stop_file is not None and args.stop_file.resolve() in {p.resolve() for p in destinations}:
        parser.error('--stop-file must differ from output, index, and state paths')
    if any(p.exists() for p in destinations):
        parser.error('Refusing to overwrite existing output, index, or state')
    if args.stop_file is not None and args.stop_file.exists():
        parser.error('--stop-file already exists; use a fresh marker path')

    try:
        import serial
    except ImportError as exc:
        parser.error(f'pyserial is required: {type(exc).__name__}')
    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = 115200
    connection.timeout = 0.1
    connection.dtr = False
    connection.rts = False
    try:
        connection.open()
        if args.reset:
            # esptool HardReset toggles the USB-serial reset lines; default is off.
            from esptool.reset import HardReset
            HardReset(connection)()
        result = capture_loop(
            connection, args.output, args.index, args.state,
            port=args.port, max_seconds=args.max_seconds,
            stop_file=args.stop_file, post_stop_seconds=args.post_stop_seconds,
            reset=args.reset,
        )
    except BaseException:
        # capture_loop owns close once called; reset/import failures occur before it.
        try:
            connection.close()
        except Exception:
            pass
        raise
    print(f"Captured {result['bytes_captured']} bytes in {result['elapsed_ms']} ms; stop={result['stop_reason']}.")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
