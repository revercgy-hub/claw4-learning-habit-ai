import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import network_overlay as overlay


class NetworkOverlayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.spec = self.root / 'spec'
        self.spec.mkdir()
        self.original = self.root / 'managed_components' / overlay.PACKAGE
        self.original.mkdir(parents=True)
        (self.original / 'station.cc').write_bytes(b'original\n')
        (self.spec / 'saved_network_policy.h').write_bytes(b'policy\n')
        self.config = dict(original_sha256=overlay.inventory(self.original),
                           replacements={'station.cc': [['original', 'patched']]})
        self.write_spec()
        self.mock = patch.object(overlay, 'SPEC', self.spec)
        self.mock.start()
        self.addCleanup(self.mock.stop)

    def write_spec(self):
        (self.spec / 'overlay.json').write_text(json.dumps(self.config))

    def test_idempotent_and_original_untouched(self):
        before = overlay.inventory(self.original)
        self.assertEqual(overlay.prepare(self.root), overlay.prepare(self.root))
        self.assertEqual(before, overlay.inventory(self.original))

    def test_dependency_drift_rejected_before_writing(self):
        (self.original / 'station.cc').write_text('different')
        with self.assertRaises(ValueError): overlay.prepare(self.root)
        self.assertFalse((self.root / 'components').exists())

    def test_manual_edit_is_never_overwritten(self):
        overlay.prepare(self.root)
        p = self.root / 'components' / overlay.PACKAGE / 'station.cc'
        p.write_text('local change')
        with self.assertRaises(ValueError): overlay.prepare(self.root)
        self.assertEqual(p.read_text(), 'local change')

    def test_extra_file_rejected(self):
        overlay.prepare(self.root)
        (self.root / 'components' / overlay.PACKAGE / 'extra').write_text('x')
        with self.assertRaises(ValueError): overlay.verify(self.root)

    def test_missing_or_duplicate_anchor_rejected(self):
        for before in ['absent', '']:
            self.config['replacements'] = {'station.cc': [[before, 'new']]}
            self.write_spec()
            with self.assertRaises(ValueError): overlay.prepare(self.root)

    def test_policy_drift_rejected(self):
        overlay.prepare(self.root)
        (self.spec / 'saved_network_policy.h').write_text('new policy')
        with self.assertRaises(ValueError): overlay.verify(self.root)
