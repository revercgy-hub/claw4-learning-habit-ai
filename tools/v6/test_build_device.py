import tempfile
import unittest
from pathlib import Path

from build_device import BOARD_RELATIVE, sync_board


class SyncBoardTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'staged'
        self.overlay = self.root / 'overlay'
        self.staged_board = self.source / BOARD_RELATIVE
        self.staged_board.mkdir(parents=True)
        self.overlay.mkdir()
        (self.overlay / 'claw4_board.cc').write_bytes(b'authoritative board')
        (self.staged_board / 'claw4_board.cc').write_bytes(b'stale board')

    def test_syncs_authoritative_content(self):
        count = sync_board(self.source, self.overlay)
        self.assertEqual(count, 1)
        self.assertEqual((self.staged_board / 'claw4_board.cc').read_bytes(),
                         b'authoritative board')

    def test_inventory_mismatch_fails_before_copy(self):
        (self.staged_board / 'local-change.cc').write_bytes(b'preserve')
        with self.assertRaisesRegex(ValueError, 'inventory differs'):
            sync_board(self.source, self.overlay)
        self.assertEqual((self.staged_board / 'claw4_board.cc').read_bytes(),
                         b'stale board')

    def test_missing_stage_fails_closed(self):
        (self.staged_board / 'claw4_board.cc').unlink()
        self.staged_board.rmdir()
        with self.assertRaisesRegex(ValueError, 'Missing staged board directory'):
            sync_board(self.source, self.overlay)


if __name__ == '__main__':
    unittest.main()
