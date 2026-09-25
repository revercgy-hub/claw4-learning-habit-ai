import tempfile
import unittest
from pathlib import Path

from stage_device import NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED, replace_once


class NvsPolicyPatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / 'main.cc'

    def test_exact_upstream_source_fails_closed_without_erase(self):
        original = '#include <nvs_flash.h>\nvoid app_main() {\n' + NVS_ERASE_ANCHOR + '\n}\n'
        self.source.write_text(original, encoding='utf-8')
        replace_once(self.source, NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED)
        result = self.source.read_text(encoding='utf-8')
        self.assertEqual(result, original.replace(NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED))
        self.assertNotIn('nvs_flash_erase(', result)
        self.assertIn('ESP_ERROR_CHECK(ret);', result)

    def test_anchor_drift_refuses_without_writing(self):
        drifted = NVS_ERASE_ANCHOR.replace('ESP_ERR_NVS_NEW_VERSION_FOUND', 'ESP_ERR_NVS_UNKNOWN')
        self.source.write_text(drifted, encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Upstream anchor drift'):
            replace_once(self.source, NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED)
        self.assertEqual(self.source.read_text(encoding='utf-8'), drifted)

    def test_ambiguous_anchor_refuses_without_writing(self):
        duplicate = NVS_ERASE_ANCHOR + '\n' + NVS_ERASE_ANCHOR
        self.source.write_text(duplicate, encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Upstream anchor drift'):
            replace_once(self.source, NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED)
        self.assertEqual(self.source.read_text(encoding='utf-8'), duplicate)


if __name__ == '__main__':
    unittest.main()
