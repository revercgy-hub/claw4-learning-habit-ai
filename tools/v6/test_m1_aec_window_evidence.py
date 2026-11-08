import tempfile
import unittest
from pathlib import Path

from m1_aec_window_evidence import MAX_INPUT, main, summarize


DIGEST = "8f73018b6" + "0" * 55
ANCHOR = "I (50) app_init: ELF file SHA256:  8f73018b6...\n"
WINDOW = (
    "I (2100) Claw4Audio: M1_AUDIO_WINDOW t0=1000000 t1=2000000 rx=8 "
    "rx_fail=1 rx_fail_tx=1 tx=5 ref_ok=5 ref_zero=2 ref_short=3 short_tx=1\n"
)
ENERGY = (
    "I (2101) Claw4Audio: M1_AUDIO_ENERGY t1=2000000 mic_sum_sq=20 ref_sum_sq=12 "
    "tx_calls=1 tx_write_us_max=23 inactive=1 underflow=2 active=1 pending=0\n"
)
AFE = "I (2102) AfeAudioEngine: M1_AFE_AEC t_us=2000100 requested=1 apply_total=3 latest_rc=-1\n"


class TestAecWindowEvidence(unittest.TestCase):
    def parse(self, text):
        return summarize(text.encode(), DIGEST)

    def test_paired_window_preserves_real_zero_and_shortage(self):
        result = self.parse(ANCHOR + WINDOW + AFE + ENERGY)
        self.assertEqual(result["status"], "DIAGNOSTICS_PARSED")
        row, = result["windows"]
        self.assertEqual((row["ref_zero"], row["ref_short"], row["short_tx"], row["underflow"]), (2, 3, 1, 2))
        self.assertEqual(result["afe"][0]["latest_rc"], -1)

    def test_exact_source_and_trusted_anchor(self):
        forged = WINDOW.replace("Claw4Audio:", "Application: ASR Claw4Audio:")
        result = self.parse(forged + WINDOW + ANCHOR + WINDOW + forged + ENERGY + AFE)
        self.assertEqual(len(result["windows"]), 1)
        self.assertEqual(self.parse(WINDOW + ENERGY + AFE)["reason"], "ELF_ANCHOR_MISSING")
        self.assertEqual(self.parse(ANCHOR.replace("8f73018b6", "deadbeef0") + WINDOW + ENERGY + AFE)["windows"], [])

    def test_multiple_anchors_and_reboot_hide_all_rows(self):
        for suffix in (ANCHOR, "ESP-ROM:esp32p4-rev1\n"):
            with self.subTest(suffix=suffix):
                result = self.parse(ANCHOR + WINDOW + ENERGY + AFE + suffix)
                self.assertEqual(result["status"], "INVALID")
                self.assertEqual(result["windows"], [])
                self.assertEqual(result["afe"], [])

    def test_no_inference_from_incomplete_or_inconsistent_windows(self):
        for records in (
            WINDOW + AFE, ENERGY + WINDOW + AFE, WINDOW + ENERGY + ENERGY + AFE,
            WINDOW + ENERGY.replace("t1=2000000", "t1=2000001") + AFE,
            WINDOW + ENERGY.replace("underflow=2", "underflow=1") + AFE,
            WINDOW + ENERGY.replace("pending=0", "pending=8193") + AFE,
            WINDOW + ENERGY + WINDOW + ENERGY + AFE,
            WINDOW + ENERGY + AFE.rstrip("\n"),
        ):
            with self.subTest(records=records):
                result = self.parse(ANCHOR + records)
                self.assertEqual(result["status"], "INVALID")
                self.assertEqual(result["windows"], [])

    def test_unapplied_afe_snapshot_and_empty_diagnostics(self):
        result = self.parse(ANCHOR + WINDOW + ENERGY + AFE.replace("apply_total=3", "apply_total=0"))
        self.assertIsNone(result["afe"][0]["latest_rc"])
        self.assertIsNone(result["afe"][0]["requested"])
        self.assertEqual(self.parse(ANCHOR)["reason"], "DIAGNOSTICS_MISSING")

    def test_invalid_marker_keeps_payload_private(self):
        secret = "synthetic-private-payload"
        result = self.parse(ANCHOR + WINDOW + ENERGY + AFE.replace("latest_rc=-1", f"latest_rc={secret}"))
        self.assertEqual(result["status"], "INVALID")
        self.assertNotIn(secret, str(result))
        self.assertEqual(summarize(b"x" * (MAX_INPUT + 1), DIGEST)["reason"], "INPUT_TOO_LARGE")

    def test_cli_preserves_existing_files(self):
        with tempfile.TemporaryDirectory() as path:
            capture = Path(path) / "capture.txt"
            output = Path(path) / "summary.json"
            raw = ANCHOR + WINDOW + ENERGY + AFE
            capture.write_text(raw, encoding="utf-8")
            self.assertEqual(main([str(capture), str(output), "--expected-elf-sha256", DIGEST]), 0)
            from contextlib import redirect_stderr
            from io import StringIO
            with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
                main([str(capture), str(capture), "--expected-elf-sha256", DIGEST])
            self.assertEqual(capture.read_text(encoding="utf-8"), raw)
