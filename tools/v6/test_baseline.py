import subprocess
import tempfile
import unittest
from pathlib import Path
import baseline


class BoundaryTests(unittest.TestCase):
    def fixture(self, root, header):
        (root / 'integration/v6/learning_core').mkdir(parents=True)
        (root / 'firmware/main').mkdir(parents=True)
        (root / 'integration/v6/learning_core/CMakeLists.txt').write_text('"${CORE}/entry.cpp"')
        (root / 'firmware/main/entry.cpp').write_text('#include "nested.h"\n')
        (root / 'firmware/main/nested.h').write_text(header)

    def test_transitive_legacy_voice_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, 'void f() { abortCloudReply(); }')
            with self.assertRaises(ValueError):
                baseline.verify_core(root)

    def test_platform_include_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, '#include <freertos/task.h>')
            with self.assertRaises(ValueError):
                baseline.verify_core(root)

    def test_cycle_and_comment_allowed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, '// Do not use VoiceSessionPort\n#include "entry.cpp"')
            self.assertEqual(baseline.verify_core(root), (1, 2))

    def test_unresolved_include_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, '#include "missing.h"')
            with self.assertRaises(ValueError):
                baseline.verify_core(root)

    def test_dirty_or_wrong_checkout_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(['git', 'init', '-q', str(root)], check=True)
            (root / 'file').write_text('baseline')
            baseline.git(root, 'add', 'file')
            baseline.git(root, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'fixture')
            sha = baseline.git(root, 'rev-parse', 'HEAD')
            baseline.verify_checkout(root, sha)
            with self.assertRaises(ValueError):
                baseline.verify_checkout(root, '0' * 40)
            (root / 'untracked').write_text('unreviewed input')
            with self.assertRaises(ValueError):
                baseline.verify_checkout(root, sha)


if __name__ == '__main__':
    unittest.main()
