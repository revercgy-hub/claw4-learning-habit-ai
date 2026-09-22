import hashlib
import tempfile
import unittest
from pathlib import Path
from audio_evidence import summarize


class AudioEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.manifest = dict(elf_sha256='a' * 64, captures=[])

    def add(self, name, session, text):
        data = text.encode()
        (self.base / name).write_bytes(data)
        self.manifest['captures'].append(dict(path=name, session=session,
                                             sha256=hashlib.sha256(data).hexdigest()))

    def boot(self, name='a', session='one'):
        self.add(name, session, f'I (9) test: capture={name}\n' + 'I (10) V6M0: CANDIDATE_ELF_SHA256=aaaaaaaaa\n'
                 'I (20) V6M0: WAKE_DETECTED (local only) count=7\n'
                 'I (21) V6M0: HEALTH free=1\n')

    def test_counts_observed_events_not_counter_deltas_or_maxima(self):
        self.boot()
        self.add('b', 'one', 'I (30) V6M0: WAKE_DETECTED (local only) count=9\n'
                 'I (31) V6M0: HEALTH free=1\n')
        self.boot('c', 'two')
        result = summarize(self.manifest, self.base)
        self.assertEqual(result['wake_events_observed'], 3)
        self.assertEqual(result['health_samples_observed'], 3)

    def test_unbound_continuation_rejected(self):
        self.add('a', 'one', 'I (20) V6M0: HEALTH free=1\n')
        with self.assertRaises(ValueError): summarize(self.manifest, self.base)

    def test_duplicate_hash_rejected(self):
        self.boot()
        self.manifest['captures'] *= 2
        with self.assertRaises(ValueError): summarize(self.manifest, self.base)

    def test_wrong_candidate_rejected(self):
        self.boot()
        self.manifest['elf_sha256'] = 'b' * 64
        with self.assertRaises(ValueError): summarize(self.manifest, self.base)

    def test_overlap_and_reboot_rejected(self):
        for text in ['I (15) V6M0: HEALTH free=1\n',
                     'I (30) V6M0: CANDIDATE_ELF_SHA256=aaaaaaaaa\n']:
            with self.subTest(text=text):
                self.manifest['captures'] = []
                self.boot()
                self.add('b', 'one', text)
                with self.assertRaises(ValueError): summarize(self.manifest, self.base)

    def test_idle_zero_reference_not_failure(self):
        self.boot()
        self.add('b', 'one', 'I (30) Claw4Audio: INPUT ch=1 n=16000 rms=0 peak=0 clipped=0 raw_peak=0\n')
        result = summarize(self.manifest, self.base)
        self.assertEqual(result['channels']['1']['samples'], 16000)
        self.assertEqual(result['reference_status'], 'HARDWARE_VERIFY_REQUIRED')

    def test_tamper_rejected(self):
        self.boot()
        (self.base / 'a').write_text('changed')
        with self.assertRaises(ValueError): summarize(self.manifest, self.base)
