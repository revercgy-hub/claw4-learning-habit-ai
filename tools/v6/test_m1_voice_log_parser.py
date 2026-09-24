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


if __name__ == "__main__":
    unittest.main()
