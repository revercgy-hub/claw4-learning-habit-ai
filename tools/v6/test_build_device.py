import tempfile
import unittest
from pathlib import Path

from build_device import (BOARD_RELATIVE, expected_local_component_lock,
                          sync_board, verify_component_lock)
from baseline import ROOT


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

    def test_m1_component_lock_refuses_drift(self):
        expected = self.root / 'reviewed.lock'
        actual = self.source / 'dependencies.lock'
        expected.write_bytes(b'component: pinned\n')
        actual.write_bytes(expected.read_bytes())
        verify_component_lock(self.source, expected)
        actual.write_bytes(b'component: newer\n')
        with self.assertRaisesRegex(ValueError, 'component lock drift'):
            verify_component_lock(self.source, expected)

    def test_m1_local_override_lock_allows_only_reviewed_rewrite(self):
        expected = ROOT / 'integration/v6/idf61-components.lock'
        actual = self.source / 'dependencies.lock'
        manifest_hash = 'a' * 64
        reviewed = expected_local_component_lock(
            expected.read_text(encoding='utf-8'), manifest_hash)
        actual.write_text(reviewed, encoding='utf-8')
        verify_component_lock(self.source, expected, local_override=True)
        for changed in (reviewed.replace('version: 1.6.4', 'version: 1.6.5'),
                        reviewed.replace('type: local', 'type: service'),
                        reviewed.replace('path: components/78__esp-wifi-connect',
                                         'path: components/other')):
            actual.write_text(changed, encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'component lock drift'):
                verify_component_lock(self.source, expected, local_override=True)


if __name__ == '__main__':
    unittest.main()
