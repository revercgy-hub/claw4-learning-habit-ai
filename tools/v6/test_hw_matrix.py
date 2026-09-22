"""Unit tests for the V6 M0 hardware-matrix collector.

All fixtures are synthetic. No private device log is read, and no test depends on
the developer machine's out/v6-device-private/ directory.
"""

from __future__ import annotations

import json
import os
import re
import tempfile
import unittest

import hw_matrix


SYNTHETIC_BOOT = """\
rst:0x17 (CHIP_USB_UART_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
I (91) boot:  5 assets           Unknown data     01 82 01000000 00f00000
E (6005) system_api: 0 mac type is incorrect (not found)
I (7745) transport: Identified slave [esp32c5]
I (7693) H_SDIO_DRV: Card init success, TRANSPORT_RX_ACTIVE
I (8515) RPC_WRAP: Coprocessor Boot-up
I (5584) Claw4V6: Native RGB888 display, panel double buffers, full refresh
I (5584) Claw4V6: NV3051F display initialized, native RGB888
I (5604) Claw4V6: GT911 touch initialized
I (5609) Claw4Audio: I2S slave 16kHz stereo32; mic+reference; bounded IO
I (5652) Claw4V6: LVGL first refresh completed (physical image still requires confirmation)
E (5535) lcd_panel: esp_lcd_panel_swap_xy(60): swap_xy is not supported by this panel
I (5806) V6M0: Assets applied=1
I (5838) AFE_CONFIG: Set WakeNet Model: wn9_nihaoxiaozhi_tts
I (5922) AFE: AFE Pipeline: [input] -> |AEC(FD_LOW_COST)| -> [output]
I (9565) V6M0: BOOT_READY IDF=v6.1; no protocol or OTA started
I (9565) V6M0: TOUCH count=1; audio record begin
I (9619) V6M0: NETWORK_EVENT=0
I (9599) WifiStation: Scanning all channels
I (12599) V6M0: Audio testing playback queued
I (12057) WifiStation: No AP found, next scan in 10 seconds
I (17107) V6M0: TOUCH count=4; audio record begin
I (18557) V6M0: HEALTH free=28027643 psram=27878204 wake=1 taps=1
I (28557) V6M0: HEALTH free=28027643 psram=27878204 wake=1 taps=2
"""

SYNTHETIC_FLASH = """\
Writing at 0x00200000... (100 %)
Hash of data verified.
Writing at 0x00200000... (100 %)
Hash of data verified.
"""

CANDIDATE = {
    "candidate": "claw4-learning-v6-m0.1",
    "files": {"build/xiaozhi.bin": {"sha256": "a" * 64, "bytes": 3042864}},
}


def _write(tmp: str, name: str, body: str) -> str:
    path = os.path.join(tmp, name)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(body)
    return path


class SignalTests(unittest.TestCase):
    def setUp(self):
        self._dir = tempfile.TemporaryDirectory()
        self.boot_path = _write(self._dir.name, "boot.txt", SYNTHETIC_BOOT)
        self.flash_path = _write(self._dir.name, "flash.txt", SYNTHETIC_FLASH)
        self.cand_path = os.path.join(self._dir.name, "candidate.json")
        with open(self.cand_path, "w", encoding="utf-8") as handle:
            json.dump(CANDIDATE, handle)

    def tearDown(self):
        self._dir.cleanup()

    def boot(self):
        return hw_matrix.parse_boot_log(self.boot_path)

    def test_timestamped_and_preamble_lines(self):
        parsed = self.boot()
        self.assertEqual(parsed["last_timestamp_ms"], 28557)
        self.assertTrue(parsed["preamble"][0].startswith("rst:0x17"))

    def test_boot_and_version_signals(self):
        s = self.boot()["signals"]
        self.assertEqual(s["idf_version"], "v6.1")
        self.assertIn("BOOT_READY", s["boot_ready"])

    def test_counts_and_ints(self):
        s = self.boot()["signals"]
        self.assertEqual(s["assets_applied"], 1)
        # touch_last_count is last-wins: 1 then 4 -> 4
        self.assertEqual(s["touch_last_count"], 4)
        self.assertEqual(s["touch_record_cycles"], 2)
        self.assertEqual(s["playback_queued"], 1)
        self.assertEqual(s["wifi_scan_cycles"], 1)
        self.assertEqual(s["wifi_no_ap"], 1)
        self.assertEqual(s["net_event"], 0)
        self.assertEqual(s["mac_not_ready"], 1)
        self.assertEqual(s["panel_capability_errors"], 1)

    def test_health_samples_carry_real_semantics(self):
        health = self.boot()["health"]
        self.assertEqual(len(health), 2)
        self.assertEqual(health[0]["psram"], 27878204)
        self.assertEqual(health[1]["taps"], 2)
        # wake=1 is IsWakeWordRunning(), i.e. the detector is armed.
        self.assertEqual(health[0]["wake_word_running"], 1)

    def test_absent_signal_is_none_not_zero(self):
        s = self.boot()["signals"]
        for key in ("wifi_connected", "wifi_config_mode", "wake_detected"):
            self.assertIn(key, s)
            self.assertEqual(s[key], 0 if key != "wifi_connected" else None)

    def test_flash_log_counts_verified_images(self):
        parsed = hw_matrix.parse_flash_log(self.flash_path)
        self.assertEqual(parsed["verified_images"], 2)
        self.assertEqual(len(parsed["sha256"]), 64)


class MatrixTests(unittest.TestCase):
    def setUp(self):
        self._dir = tempfile.TemporaryDirectory()
        self.boot_path = _write(self._dir.name, "boot.txt", SYNTHETIC_BOOT)
        self.flash_path = _write(self._dir.name, "flash.txt", SYNTHETIC_FLASH)
        self.cand_path = os.path.join(self._dir.name, "candidate.json")
        with open(self.cand_path, "w", encoding="utf-8") as handle:
            json.dump(CANDIDATE, handle)
        self.matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(self.boot_path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            dict(hw_matrix.DEFAULT_USER_CONFIRMATIONS),
        )

    def tearDown(self):
        self._dir.cleanup()

    def status(self, item: str) -> str:
        for row in self.matrix["rows"]:
            if row["item"] == item:
                return row["status"]
        raise AssertionError(f"missing row: {item}")

    def test_confirmed_items_pass(self):
        self.assertEqual(self.status("启动"), "PASS")
        self.assertEqual(self.status("显示"), "PASS")
        self.assertEqual(self.status("触摸"), "PASS")
        self.assertEqual(self.status("音频"), "PASS")
        self.assertEqual(self.status("C5 链路"), "PASS")

    def test_audio_loopback_must_not_promote_wake_word(self):
        """The core invariant: a confirmed playback loopback says nothing about
        wake-word detection, which needs a positive WAKE_DETECTED event."""
        self.assertEqual(self.status("音频"), "PASS")
        self.assertEqual(self.status("唤醒词"), "NOT_VERIFIED")

    def test_network_is_not_verified_without_ip(self):
        self.assertEqual(self.status("网络 / IP"), "NOT_VERIFIED")
        self.assertEqual(self.matrix["signals"]["wifi_no_ap"], 1)

    def test_rollback_stays_unproven(self):
        self.assertEqual(self.status("回滚（恢复写回）"), "NOT_TESTED")

    def test_display_fails_without_user_confirmation(self):
        confirmations = dict(hw_matrix.DEFAULT_USER_CONFIRMATIONS)
        confirmations["display_visible"] = False
        matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(self.boot_path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            confirmations,
        )
        row = next(r for r in matrix["rows"] if r["item"] == "显示")
        self.assertNotEqual(row["status"], "PASS")

    def test_boot_fails_when_assertions_present(self):
        noisy = SYNTHETIC_BOOT + "assert failed: xTaskGetSchedulerState scheduler.c:123\n"
        path = _write(self._dir.name, "boot-assert.txt", noisy)
        matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            dict(hw_matrix.DEFAULT_USER_CONFIRMATIONS),
        )
        row = next(r for r in matrix["rows"] if r["item"] == "启动")
        self.assertNotEqual(row["status"], "PASS")
        self.assertEqual(matrix["signals"]["assertions"], 1)

    def test_render_markdown_contains_every_row(self):
        md = hw_matrix.render_markdown(self.matrix)
        for row in self.matrix["rows"]:
            self.assertIn(row["item"], md)
            self.assertIn(row["status"], md)
        self.assertIn("HEALTH 采样", md)

    def test_render_escapes_pipes_that_would_break_the_table(self):
        md = hw_matrix.render_markdown(self.matrix)
        # The AFE pipeline string contains literal '|'; it must not split a cell,
        # so the row may only carry the three structural delimiters unescaped.
        line = next(l for l in md.splitlines() if l.startswith("| `afe_pipeline`"))
        unescaped = len(re.findall(r"(?<!\\)\|", line))
        self.assertEqual(unescaped, 3, line)

    def test_health_is_aggregated_across_captures(self):
        self.assertEqual(len(self.matrix["health_all"]), 2)
        self.assertTrue(all("log" in h for h in self.matrix["health_all"]))

    def test_render_warns_when_health_is_missing(self):
        matrix = dict(self.matrix)
        matrix["health_all"] = []
        md = hw_matrix.render_markdown(matrix)
        self.assertIn("证据缺口", md)


if __name__ == "__main__":
    unittest.main()
