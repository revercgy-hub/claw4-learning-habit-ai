import json
from pathlib import Path
import tempfile
import unittest

from m1_continuous_capture import capture_loop


class FakeClock:
    def __init__(self):
        self.value = 0.0

    def monotonic(self):
        return self.value

    def advance(self, seconds):
        self.value += seconds


class FakeSerial:
    def __init__(self, clock, chunks, stop_file=None, stop_after_reads=None, fail=False):
        self.clock = clock
        self.chunks = iter(chunks)
        self.stop_file = stop_file
        self.stop_after_reads = stop_after_reads
        self.fail = fail
        self.reads = 0
        self.closed = False
        self.in_waiting = 64

    def read(self, size):
        self.clock.advance(0.25)
        self.reads += 1
        if self.stop_file is not None and self.reads == self.stop_after_reads:
            self.stop_file.write_text('private marker contents must not escape', encoding='utf-8')
        if self.fail:
            raise OSError('serial read failed with private payload')
        return next(self.chunks, b'')

    def close(self):
        self.closed = True


class ContinuousCaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.output = self.root / 'private.raw'
        self.index = self.root / 'capture.jsonl'
        self.state = self.root / 'capture-state.json'

    def tearDown(self):
        self.temp.cleanup()

    def run_capture(self, connection, clock, **kwargs):
        return capture_loop(
            connection, self.output, self.index, self.state,
            port='COM7', max_seconds=kwargs.pop('max_seconds', 2),
            monotonic=clock.monotonic,
            utc_now=lambda: '2026-09-27T00:00:00.000Z', **kwargs,
        )

    def test_contiguous_offsets_and_time_index(self):
        clock = FakeClock()
        connection = FakeSerial(clock, [b'one', b'two!', b'last'])
        state = self.run_capture(connection, clock)
        self.assertEqual(self.output.read_bytes(), b'onetwo!last')
        records = [json.loads(line) for line in self.index.read_text(encoding='utf-8').splitlines()]
        self.assertEqual([r['end_offset'] for r in records], [3, 7, 11])
        self.assertEqual([r['bytes'] for r in records], [3, 4, 4])
        self.assertEqual([r['elapsed_ms'] for r in records], [250, 500, 750])
        self.assertEqual({r['utc'] for r in records}, {'2026-09-27T00:00:00.000Z'})
        self.assertEqual(state['bytes_captured'], 11)
        self.assertTrue(connection.closed)

    def test_stop_marker_timing_includes_bounded_post_window(self):
        clock = FakeClock()
        marker = self.root / 'STOP'
        connection = FakeSerial(clock, [b'A', b'B', b'C', b'D', b'E'], marker, 1)
        state = self.run_capture(connection, clock, stop_file=marker, post_stop_seconds=0.5)
        records = [json.loads(line) for line in self.index.read_text(encoding='utf-8').splitlines()]
        self.assertEqual([r['end_offset'] for r in records], [1, 2, 3])
        self.assertEqual(state['stop_reason'], 'stop_file')
        self.assertEqual(state['stop_seen_elapsed_ms'], 250)
        self.assertEqual(state['elapsed_ms'], 750)
        self.assertNotIn('private marker contents', self.index.read_text(encoding='utf-8'))
        self.assertNotIn('private marker contents', self.state.read_text(encoding='utf-8'))
        self.assertTrue(connection.closed)

    def test_max_duration_stops_and_payload_is_not_indexed_or_state_metadata(self):
        clock = FakeClock()
        secretish_payload = b'credential=sensitive speech transcript'
        connection = FakeSerial(clock, [secretish_payload, b'next', b'late'])
        state = self.run_capture(connection, clock, max_seconds=1)
        self.assertEqual(state['stop_reason'], 'max_seconds')
        self.assertEqual(state['elapsed_ms'], 1000)
        self.assertEqual(self.output.read_bytes(), secretish_payload + b'next' + b'late')
        for metadata in (self.index.read_text(encoding='utf-8'), self.state.read_text(encoding='utf-8')):
            self.assertNotIn('credential', metadata)
            self.assertNotIn('sensitive speech', metadata)
            self.assertNotIn('transcript', metadata)

    def test_refuses_overwrite_and_closes_connection(self):
        self.output.write_bytes(b'preserve')
        clock = FakeClock()
        connection = FakeSerial(clock, [])
        with self.assertRaises(FileExistsError):
            self.run_capture(connection, clock)
        self.assertTrue(connection.closed)
        self.assertEqual(self.output.read_bytes(), b'preserve')

    def test_refuses_old_stop_marker_and_closes_connection(self):
        marker = self.root / 'STOP'
        marker.write_text('do not read', encoding='utf-8')
        clock = FakeClock()
        connection = FakeSerial(clock, [])
        with self.assertRaisesRegex(FileExistsError, 'Stop-file already exists'):
            self.run_capture(connection, clock, stop_file=marker)
        self.assertTrue(connection.closed)

    def test_validation_failure_and_stop_path_collision_close_connection(self):
        clock = FakeClock()
        connection = FakeSerial(clock, [])
        with self.assertRaisesRegex(ValueError, 'max_seconds'):
            self.run_capture(connection, clock, max_seconds=601)
        self.assertTrue(connection.closed)

        clock = FakeClock()
        connection = FakeSerial(clock, [])
        with self.assertRaisesRegex(ValueError, 'Stop-file path'):
            capture_loop(connection, self.output, self.index, self.state,
                         port='COM7', max_seconds=2, stop_file=self.output,
                         monotonic=clock.monotonic, utc_now=lambda: '2026-09-27T00:00:00Z')
        self.assertTrue(connection.closed)

    def test_closes_connection_and_leaves_payload_free_error_state_on_exception(self):
        clock = FakeClock()
        connection = FakeSerial(clock, [], fail=True)
        with self.assertRaisesRegex(OSError, 'serial read failed'):
            self.run_capture(connection, clock)
        self.assertTrue(connection.closed)
        state = json.loads(self.state.read_text(encoding='utf-8'))
        self.assertEqual(state['status'], 'error')
        self.assertEqual(state['error_type'], 'OSError')
        self.assertNotIn('private payload', self.state.read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
