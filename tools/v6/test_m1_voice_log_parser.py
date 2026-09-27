import json
import unittest
from pathlib import Path
from unittest.mock import patch

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

    def test_m1_afe_diagnostics_aggregate_counters_and_keep_latest_state(self):
        result = summarize("""I (123) AfeAudioEngine: M1_AFE_STATE wake=1 active=1 wn_on=1 wn_off=0 detected=2 other=3
I (123) AfeAudioEngine: M1_AFE_FLOW feed_calls=10 feed_samples=1600 feed_chunks=4 fetch_ok=9 fetch_fail=1 fetch_other=0
W (124) AfeAudioEngine: M1_AFE_STATE wake=0 active=1 wn_on=0 wn_off=0 detected=0 other=1
W (124) AfeAudioEngine: M1_AFE_FLOW feed_calls=11 feed_samples=1700 feed_chunks=5 fetch_ok=10 fetch_fail=0 fetch_other=1
""")
        self.assertEqual(result["m1_afe_state_sample_count"], 2)
        self.assertEqual(result["m1_afe_wake_enabled_last"], 0)
        self.assertEqual(result["m1_afe_active_last"], 1)
        self.assertEqual(result["m1_afe_wn_on_total"], 1)
        self.assertEqual(result["m1_afe_wn_off_total"], 0)
        self.assertEqual(result["m1_afe_detected_total"], 2)
        self.assertEqual(result["m1_afe_other_total"], 4)
        self.assertEqual(result["m1_afe_flow_sample_count"], 2)
        self.assertEqual(result["m1_afe_feed_calls_total"], 21)
        self.assertEqual(result["m1_afe_feed_samples_total"], 3300)
        self.assertEqual(result["m1_afe_feed_chunks_total"], 9)
        self.assertEqual(result["m1_afe_fetch_ok_total"], 19)
        self.assertEqual(result["m1_afe_fetch_fail_total"], 1)
        self.assertEqual(result["m1_afe_fetch_other_total"], 1)
        self.assertTrue(all(value == "NOT_VERIFIED" for value in result["voice_flow"].values()))

    def test_m1_afe_parser_rejects_wrong_tag_malformed_embedded_and_out_of_range(self):
        result = summarize("""I (1) OtherTag: M1_AFE_STATE wake=1 active=1 wn_on=1 wn_off=0 detected=2 other=3
I (2) AfeAudioEngine: M1_AFE_STATE wake=1 active=1 wn_on=1 wn_off=0 detected=2 other=3 trailing
I (3) AfeAudioEngine: M1_AFE_STATE wake=1 active=1 wn_on=1 wn_off=0 detected=4294967296 other=3
I (4) AfeAudioEngine: M1_AFE_FLOW feed_calls=1 feed_samples=2 feed_chunks=3 fetch_ok=4 fetch_fail=5
I (5) AfeAudioEngine: M1_AFE_FLOW feed_calls=4294967296 feed_samples=2 feed_chunks=3 fetch_ok=4 fetch_fail=5 fetch_other=6
I (6) Application: speech says AfeAudioEngine: M1_AFE_STATE wake=1 active=1 wn_on=1 wn_off=0 detected=2 other=3
""" + "I (7) AfeAudioEngine: M1_AFE_FLOW feed_calls=1 feed_samples=2 feed_chunks=3 fetch_ok=4 fetch_fail=5 fetch_other=6" + "x" * 300)
        self.assertEqual(result["m1_afe_state_sample_count"], 0)
        self.assertIsNone(result["m1_afe_wake_enabled_last"])
        self.assertIsNone(result["m1_afe_active_last"])
        self.assertEqual(result["m1_afe_wn_on_total"], 0)
        self.assertEqual(result["m1_afe_flow_sample_count"], 0)
        self.assertEqual(result["m1_afe_feed_calls_total"], 0)

    def test_m1_afe_state_rejects_non_boolean_gauges_atomically(self):
        result = summarize("""I (1) AfeAudioEngine: M1_AFE_STATE wake=1 active=0 wn_on=2 wn_off=1 detected=3 other=4
I (2) AfeAudioEngine: M1_AFE_STATE wake=2 active=1 wn_on=9 wn_off=8 detected=7 other=6
""")
        self.assertEqual(result["m1_afe_state_sample_count"], 1)
        self.assertEqual(result["m1_afe_wake_enabled_last"], 1)
        self.assertEqual(result["m1_afe_active_last"], 0)
        self.assertEqual(result["m1_afe_wn_on_total"], 2)
        self.assertEqual(result["m1_afe_wn_off_total"], 1)
        self.assertEqual(result["m1_afe_detected_total"], 3)
        self.assertEqual(result["m1_afe_other_total"], 4)

    def test_m1_afe_totals_overflow_fail_closed_without_partial_record(self):
        logs = """I (1) AfeAudioEngine: M1_AFE_STATE wake=1 active=1 wn_on=9 wn_off=0 detected=0 other=0
I (1) AfeAudioEngine: M1_AFE_STATE wake=0 active=0 wn_on=2 wn_off=0 detected=0 other=0
I (1) AfeAudioEngine: M1_AFE_FLOW feed_calls=8 feed_samples=1 feed_chunks=0 fetch_ok=0 fetch_fail=0 fetch_other=0
I (1) AfeAudioEngine: M1_AFE_FLOW feed_calls=3 feed_samples=2 feed_chunks=0 fetch_ok=0 fetch_fail=0 fetch_other=0
"""
        with patch("tools.v6.m1_voice_log_parser.MAX_U64", 10):
            result = summarize(logs)
        self.assertEqual(result["m1_afe_state_sample_count"], 1)
        self.assertEqual(result["m1_afe_wake_enabled_last"], 1)
        self.assertEqual(result["m1_afe_wn_on_total"], 9)
        self.assertEqual(result["m1_afe_flow_sample_count"], 1)
        self.assertEqual(result["m1_afe_feed_calls_total"], 8)
        self.assertEqual(result["m1_afe_feed_samples_total"], 1)
        self.assertTrue(all(value == "NOT_VERIFIED" for value in result["voice_flow"].values()))

    def test_m1_afe_control_keeps_last_valid_snapshot_without_interpreting_rc(self):
        result = summarize("""I (123) AfeAudioEngine: M1_AFE_CTL desired=7 applied=6 on_total=3 on_rc=-1 off_total=2 off_rc=0
W (124) AfeAudioEngine: M1_AFE_CTL desired=8 applied=8 on_total=4 on_rc=0 off_total=3 off_rc=-2147483648
""")
        self.assertEqual(result["m1_afe_control_sample_count"], 2)
        self.assertEqual(result["m1_afe_control_last"], {
            "desired": 8,
            "applied": 8,
            "on_total": 4,
            "on_rc": 0,
            "off_total": 3,
            "off_rc": -2147483648,
        })
        self.assertTrue(all(value == "NOT_VERIFIED" for value in result["voice_flow"].values()))

    def test_m1_afe_control_rejects_wrong_tag_malformed_embedded_and_out_of_range(self):
        valid = "I (1) AfeAudioEngine: M1_AFE_CTL desired=1 applied=1 on_total=1 on_rc=0 off_total=0 off_rc=0"
        result = summarize(valid + "\n" + """I (1) OtherTag: M1_AFE_CTL desired=1 applied=1 on_total=1 on_rc=0 off_total=0 off_rc=0
I (2) AfeAudioEngine: M1_AFE_CTL desired=1 applied=1 on_total=1 on_rc=0 off_total=0 off_rc=0 trailing
I (3) AfeAudioEngine: M1_AFE_CTL applied=1 desired=1 on_total=1 on_rc=0 off_total=0 off_rc=0
I (4) AfeAudioEngine: M1_AFE_CTL desired=4294967296 applied=1 on_total=1 on_rc=0 off_total=0 off_rc=0
I (5) AfeAudioEngine: M1_AFE_CTL desired=1 applied=1 on_total=1 on_rc=2147483648 off_total=0 off_rc=0
I (6) AfeAudioEngine: M1_AFE_CTL desired=1 applied=1 on_total=1 on_rc=0 off_total=0 off_rc=-2147483649
I (7) Application: speech says AfeAudioEngine: M1_AFE_CTL desired=1 applied=1 on_total=1 on_rc=0 off_total=0 off_rc=0
""" + valid + "x" * 300)
        self.assertEqual(result["m1_afe_control_sample_count"], 1)
        self.assertEqual(result["m1_afe_control_last"], {
            "desired": 1,
            "applied": 1,
            "on_total": 1,
            "on_rc": 0,
            "off_total": 0,
            "off_rc": 0,
        })

    def test_m1_afe_control_preserves_snapshot_on_sample_count_overflow(self):
        logs = """I (1) AfeAudioEngine: M1_AFE_CTL desired=1 applied=1 on_total=1 on_rc=0 off_total=0 off_rc=0
I (2) AfeAudioEngine: M1_AFE_CTL desired=2 applied=2 on_total=2 on_rc=-1 off_total=1 off_rc=0
"""
        with patch("tools.v6.m1_voice_log_parser.MAX_U64", 1):
            result = summarize(logs)
        self.assertEqual(result["m1_afe_control_sample_count"], 1)
        self.assertEqual(result["m1_afe_control_last"], {
            "desired": 1,
            "applied": 1,
            "on_total": 1,
            "on_rc": 0,
            "off_total": 0,
            "off_rc": 0,
        })


if __name__ == "__main__":
    unittest.main()
