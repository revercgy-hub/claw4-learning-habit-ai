import unittest

from tools.v6.m1_voice_log_parser import summarize


class M1VoiceLogParserTests(unittest.TestCase):
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
