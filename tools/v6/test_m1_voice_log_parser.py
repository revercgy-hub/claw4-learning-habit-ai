import json
import unittest
from pathlib import Path

from tools.v6.m1_voice_log_parser import summarize


class M1VoiceLogParserTests(unittest.TestCase):
    def test_preflight_template_has_two_numbered_unverified_rows(self):
        path = Path(__file__).with_name("templates") / "m1_preflight_input.json"
        template = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(template["round_count"], 2)
        self.assertEqual([row["round"] for row in template["rounds"]], [1, 2])
        self.assertEqual(len(template["rounds"]), template["round_count"])
        for row in template["rounds"]:
            for key, value in row.items():
                if key in ("round", "stimulus_id", "heap_min_bytes", "psram_min_bytes"):
                    continue
                self.assertEqual(value, "NOT_VERIFIED", (row["round"], key))
            self.assertTrue(row["stimulus_id"].startswith("FILL_"))
            self.assertIsNone(row["heap_min_bytes"])
            self.assertIsNone(row["psram_min_bytes"])
        self.assertEqual(template["status"], "NOT_VERIFIED")
        self.assertEqual(template["same_candidate_session_config_image_confirmed"], "NOT_VERIFIED")

    def test_allowlisted_markers_are_aggregated_and_flow_stays_unverified(self):
        result = summarize("""I (1) CANDIDATE_ELF_SHA256=abcdef123
I (2) V6M0: BOOT_READY IDF=6;
I (3) V6M0: HEALTH free=123 psram=456 wake=1 taps=2
I (4) V6M0: WAKE_DETECTED (local only) count=1
""")
        self.assertEqual(result["candidate_anchor_count"], 1)
        self.assertEqual(result["boot_ready_count"], 1)
        self.assertEqual(result["wake_events_observed"], 1)
        self.assertEqual(result["heap_min_bytes"], 123)
        self.assertTrue(all(x == "NOT_VERIFIED" for x in result["voice_flow"].values()))

    def test_never_emits_unknown_lines_or_private_text(self):
        secret = "private speech password=topsecret"
        result = summarize(secret + "\nI (4) Guru Meditation Error")
        self.assertEqual(result["crash_markers"], 1)
        self.assertNotIn(secret, str(result))

    def test_missing_metrics_remain_null(self):
        result = summarize("ordinary unrecognized line")
        self.assertIsNone(result["heap_min_bytes"])
        self.assertIsNone(result["psram_min_bytes"])
        self.assertEqual(result["health_samples"], 0)

    def test_m1_diagnostics_are_aggregated_from_exact_uart_records(self):
        result = summarize("""I (123) Claw4Audio: M1_INPUT_DIAG mic_n=16000 rms=12 peak=400 i2s_read_failures=1
W (124) Claw4Audio: M1_INPUT_DIAG mic_n=20000 rms=19 peak=700 i2s_read_failures=3
I (125) Claw4Audio: M1_INPUT_DIAG mic_n=0 rms=0 peak=0 i2s_read_failures=0
I (126) Application: Wake word detected
""")
        self.assertEqual(result["m1_input_diag_window_count"], 3)
        self.assertEqual(result["m1_mic_samples_total"], 36000)
        self.assertEqual(result["m1_rms_max"], 19)
        self.assertEqual(result["m1_peak_max"], 700)
        self.assertEqual(result["m1_i2s_read_failures_total"], 4)
        self.assertEqual(result["m1_wake_log_count"], 1)
        self.assertTrue(all(value == "NOT_VERIFIED" for value in result["voice_flow"].values()))

    def test_m1_parser_rejects_malformed_and_embedded_records(self):
        result = summarize("""I (1) Claw4Audio: M1_INPUT_DIAG mic_n=10 rms=2 peak=3 i2s_read_failures=4 trailing
I (2) OtherTag: M1_INPUT_DIAG mic_n=10 rms=2 peak=3 i2s_read_failures=4
I (3) Claw4Audio: M1_INPUT_DIAG mic_n=10 rms=2 peak=3
I (4) Claw4Audio: M1_INPUT_DIAG mic_n=4294967296 rms=2 peak=3 i2s_read_failures=4
I (5) Application: transcript says Application: Wake word detected
Application: Wake word detected
""" + "I (7) Claw4Audio: M1_INPUT_DIAG mic_n=10 rms=2 peak=3 i2s_read_failures=4" + "x" * 300)
        self.assertEqual(result["m1_input_diag_window_count"], 0)
        self.assertEqual(result["m1_mic_samples_total"], 0)
        self.assertIsNone(result["m1_rms_max"])
        self.assertIsNone(result["m1_peak_max"])
        self.assertEqual(result["m1_i2s_read_failures_total"], 0)
        self.assertEqual(result["m1_wake_log_count"], 0)

    def test_missing_m1_metrics_and_wake_count_do_not_change_m0_semantics(self):
        result = summarize("I (1) V6M0: WAKE_DETECTED (local only) count=1")
        self.assertEqual(result["wake_events_observed"], 1)
        self.assertEqual(result["m1_wake_log_count"], 0)
        self.assertEqual(result["m1_input_diag_window_count"], 0)
        self.assertEqual(result["m1_mic_samples_total"], 0)
        self.assertIsNone(result["m1_rms_max"])
        self.assertIsNone(result["m1_peak_max"])
        self.assertEqual(result["m1_i2s_read_failures_total"], 0)

    def test_m1_output_contains_only_aggregates(self):
        private = "I (1) Application: Wake word detected; transcript=secret endpoint=https://private"
        result = summarize(private)
        serialized = json.dumps(result)
        self.assertNotIn("transcript=secret", serialized)
        self.assertNotIn("https://private", serialized)
        self.assertNotIn("Wake word detected", serialized)


if __name__ == "__main__":
    unittest.main()
