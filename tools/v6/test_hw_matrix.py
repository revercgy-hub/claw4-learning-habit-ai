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
I (1490) esp_psram: Found 32MB PSRAM device
I (1531) main_task: Calling app_main()
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

SYNTHETIC_CAMERA_INIT_ONLY = """\
I (1000) Claw4V6: CAMERA_DIAGNOSTIC sensor_stack_initialized=1
"""

SYNTHETIC_CAMERA_CLEAN_FRAME = """\
I (1000) Claw4V6: CAMERA_DIAGNOSTIC sensor_stack_initialized=1
I (1200) Claw4V6: CAMERA_DIAGNOSTIC frame_capture=1 width=640 height=480 format=0x00000001 bytes=307200 saved=0 uploaded=0 cleanup_error=0
"""

SYNTHETIC_CAMERA_FAILURE = """\
I (1000) Claw4V6: CAMERA_DIAGNOSTIC sensor_stack_initialized=1
W (1200) Claw4V6: CAMERA_DIAGNOSTIC frame_capture=0 stage=dequeue error=11 cleanup_error=0
"""

# Third capture of the same candidate: the AP now broadcasts its SSID, so the
# scan matches and the station associates. Same firmware, same saved channel.
SYNTHETIC_NETCONNECTED = """\
rst:0x17 (CHIP_USB_UART_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
I (8677) WifiBoard: Starting WiFi connection attempt
I (9602) WifiStation: Scanning saved channel 11
I (9623) V6M0: NETWORK_EVENT=0
I (9745) RPC_WRAP: ESP Event: StaScanDone
I (9768) WifiStation: Found AP: realme, BSSID: 9e:9e:3d:f0:8e:d7, RSSI: -39, Channel: 11, Authmode: 3
I (9769) WifiBoard: WiFi connecting to realme
I (9770) V6M0: NETWORK_EVENT=1
I (11386) RPC_WRAP: ESP Event: Station mode: Connected
I (12483) esp_netif_handlers: sta ip: 10.76.189.105, mask: 255.255.255.0, gw: 10.76.189.222
I (12484) WifiBoard: Connected to WiFi: realme
I (12488) V6M0: NETWORK_EVENT=2
I (18581) V6M0: HEALTH free=27133675 psram=26841568 wake=1 taps=0
I (28586) V6M0: HEALTH free=27133675 psram=26841568 wake=1 taps=0
"""

# A later capture of the same candidate with no taps: it must not erase the
# touch evidence carried by SYNTHETIC_BOOT, and it adds config-portal evidence.
SYNTHETIC_NETPROBE = """\
rst:0x17 (CHIP_USB_UART_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
I (8515) RPC_WRAP: Coprocessor Boot-up
I (8677) WifiBoard: Starting WiFi connection attempt
I (9601) WifiStation: Scanning saved channel 11
I (9767) WifiStation: No AP on saved channels, starting full scan
I (9768) WifiStation: Scanning all channels
I (12057) WifiStation: No AP found, next scan in 10 seconds
I (68677) WifiBoard: WiFi connection timeout, entering config mode
W (68732) WifiBoard: WiFi disconnected
I (68733) V6M0: NETWORK_EVENT=3
W (68736) StateMachine: Invalid state transition: unknown -> wifi_configuring
I (68748) WifiManager: Starting config AP
I (69131) RPC_WRAP: ESP Event: softap started
I (69151) WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9
I (69175) esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
I (69191) WifiConfigurationAp: Web server started
I (69210) WifiBoard: WiFi config mode entered
I (69210) V6M0: NETWORK_EVENT=4
I (78579) V6M0: HEALTH free=27118527 psram=26837296 wake=1 taps=0
I (88584) V6M0: HEALTH free=27118527 psram=26837296 wake=1 taps=0
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
        self.netprobe_path = _write(self._dir.name, "netprobe.txt", SYNTHETIC_NETPROBE)
        self.flash_path = _write(self._dir.name, "flash.txt", SYNTHETIC_FLASH)
        self.cand_path = os.path.join(self._dir.name, "candidate.json")
        with open(self.cand_path, "w", encoding="utf-8") as handle:
            json.dump(CANDIDATE, handle)

    def tearDown(self):
        self._dir.cleanup()

    def boot(self):
        return hw_matrix.parse_boot_log(self.boot_path)

    def netprobe(self):
        return hw_matrix.parse_boot_log(self.netprobe_path)

    def test_timestamped_and_preamble_lines(self):
        parsed = self.boot()
        self.assertEqual(parsed["last_timestamp_ms"], 28557)
        self.assertTrue(parsed["preamble"][0].startswith("rst:0x17"))

    def test_config_portal_signals(self):
        s = self.netprobe()["signals"]
        self.assertEqual(s["config_ap_ssid"],
                         "WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9")
        self.assertEqual(s["config_ap_dhcp"],
                         "DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1")
        self.assertEqual(s["softap_started"], 1)
        self.assertEqual(s["config_web_server"], 1)
        self.assertEqual(s["connect_timeout"], 1)
        self.assertEqual(s["wifi_config_mode"], 1)
        self.assertEqual(s["net_events"], [3, 4])
        self.assertEqual(s["state_machine_reject"], 1)

    def test_saved_channel_scan_is_visible(self):
        """A non-zero saved channel proves credentials reached NVS even when the
        device still cannot associate."""
        s = self.netprobe()["signals"]
        self.assertEqual(s["saved_channel_scan"], 11)
        self.assertEqual(s["no_ap_on_saved_ch"], 1)
        # The first capture predates provisioning, so it has no saved channel.
        self.assertIsNone(self.boot()["signals"]["saved_channel_scan"])

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
        self.assertEqual(s["net_events"], [0])
        self.assertEqual(s["mac_not_ready"], 1)
        self.assertEqual(s["panel_capability_errors"], 1)

    def test_candidate16_phase_tagged_probe_signals(self):
        path = _write(self._dir.name, "candidate16.txt", """\
I (17000) V6M0: TOUCH count=1; probe=1 phase=recording begin
I (20000) V6M0: LOCAL_REFERENCE_PROBE_BEGIN id=1 phase=playback wake_enabled=1; RX active; no upload
I (20100) V6M0: VAD probe=1 phase=2 speaking=1 count=1 playback_onsets=1
I (20200) V6M0: WAKE_DETECTED (local only) probe=1 phase=2 count=1
I (21000) V6M0: LOCAL_REFERENCE_PROBE_END id=1 drained=1 playback_vad_onsets=1 playback_wake_detections=1
""")
        signals = hw_matrix.parse_boot_log(path)["signals"]
        self.assertEqual(signals["touch_record_cycles"], 1)
        self.assertEqual(signals["reference_probe_begins"], 1)
        self.assertEqual(signals["reference_probe_ends"], 1)
        self.assertEqual(signals["reference_probe_vad"], 1)
        self.assertEqual(signals["reference_probe_wake"], 1)

    def test_camera_diagnostic_success_and_error_signals(self):
        clean_path = _write(self._dir.name, "camera-clean.txt", SYNTHETIC_CAMERA_CLEAN_FRAME)
        failure_path = _write(self._dir.name, "camera-failure.txt", SYNTHETIC_CAMERA_FAILURE)
        clean = hw_matrix.parse_boot_log(clean_path)["signals"]
        failure = hw_matrix.parse_boot_log(failure_path)["signals"]
        self.assertEqual(clean["camera_sensor_init"], 1)
        self.assertEqual(clean["camera_frame_capture"], 1)
        self.assertEqual(clean["camera_frame_clean"], 1)
        self.assertEqual(failure["camera_sensor_init"], 1)
        self.assertEqual(failure["camera_frame_failure"], 1)
        self.assertEqual(failure["camera_frame_clean"], 0)

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
        self.netprobe_path = _write(self._dir.name, "netprobe.txt", SYNTHETIC_NETPROBE)
        self.connected_path = _write(self._dir.name, "netconnected.txt", SYNTHETIC_NETCONNECTED)
        self.flash_path = _write(self._dir.name, "flash.txt", SYNTHETIC_FLASH)
        self.cand_path = os.path.join(self._dir.name, "candidate.json")
        with open(self.cand_path, "w", encoding="utf-8") as handle:
            json.dump(CANDIDATE, handle)
        self.matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(self.boot_path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            dict(display_visible=True, audio_loopback=True),
        )
        # Two captures of the same candidate, in chronological order.
        self.merged = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(self.boot_path),
             hw_matrix.parse_boot_log(self.netprobe_path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            dict(display_visible=True, audio_loopback=True),
        )
        # Full history: touch captures, the failed reconnect, then the success.
        self.full = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(self.boot_path),
             hw_matrix.parse_boot_log(self.netprobe_path),
             hw_matrix.parse_boot_log(self.connected_path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            dict(display_visible=True, audio_loopback=True),
        )

    def tearDown(self):
        self._dir.cleanup()

    def test_associated_station_evidence(self):
        s = hw_matrix.parse_boot_log(self.connected_path)["signals"]
        self.assertEqual(s["found_ap"], "realme")
        self.assertEqual(s["sta_ip"], "10.76.189.105")
        self.assertEqual(s["sta_gateway"], "10.76.189.222")
        self.assertEqual(s["net_events"], [0, 1, 2])
        self.assertEqual(s["sta_connected_event"], 1)
        self.assertEqual(s["wifi_connecting"], 1)
        self.assertEqual(s["wifi_no_ap"], 0)

    def test_network_row_passes_once_an_ip_is_obtained(self):
        self.assertEqual(self.status("网络 / IP"), "NOT_VERIFIED")
        self.assertEqual(self.status("网络 / IP", self.full), "PASS")
        self.assertEqual(self.full["signal_source"]["wifi_connected"], "netconnected.txt")

    def test_a_successful_connect_does_not_erase_the_earlier_failure_evidence(self):
        """The matrix must keep both stories: the AP that never matched and the
        AP that did. Merging by max on counters preserves the failure counts."""
        self.assertEqual(self.full["signals"]["wifi_no_ap"], 1)
        self.assertEqual(self.full["signals"]["wifi_config_mode"], 1)
        self.assertEqual(self.full["signals"]["net_events"], [0, 1, 2, 3, 4])

    def status(self, item: str, matrix=None) -> str:
        for row in (matrix or self.matrix)["rows"]:
            if row["item"] == item:
                return row["status"]
        raise AssertionError(f"missing row: {item}")

    def test_merge_by_max_keeps_earlier_touch_evidence(self):
        """The netprobe capture has zero taps; it must not erase tap evidence."""
        self.assertEqual(self.merged["signals"]["touch_record_cycles"], 2)
        self.assertEqual(self.status("触摸", self.merged), "PASS")
        self.assertEqual(self.merged["signals"]["touch_last_count"], 4)

    def test_network_events_merge_as_a_set_not_a_max(self):
        self.assertEqual(self.merged["signals"]["net_events"], [0, 3, 4])
        self.assertEqual(self.merged["signals"]["wifi_no_ap"], 1)

    def test_signal_source_is_recorded(self):
        src = self.merged["signal_source"]
        self.assertEqual(src["touch_record_cycles"], "boot.txt")
        self.assertEqual(src["config_ap_ssid"], "netprobe.txt")

    def test_config_portal_row_passes_only_with_softap_evidence(self):
        self.assertEqual(self.status("配网模式（热点/配置页）"), "NOT_VERIFIED")
        self.assertEqual(self.status("配网模式（热点/配置页）", self.merged), "PASS")
        # A started softap must never be promoted into a successful association.
        self.assertEqual(self.status("网络 / IP", self.merged), "NOT_VERIFIED")

    def test_numeric_signals_are_not_merged_by_simple_overwrite(self):
        """Guard against regressing to 'last capture wins', which would drop the
        touch evidence as soon as a quiet capture is appended."""
        self.assertNotEqual(self.merged["signals"]["touch_record_cycles"], 0)

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

    def camera_matrix(self, name: str, body: str):
        path = _write(self._dir.name, name, body)
        return hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(path)], [], self.cand_path, CANDIDATE, {})

    def test_camera_sensor_init_alone_does_not_pass_capture(self):
        matrix = self.camera_matrix("camera-init-only.txt", SYNTHETIC_CAMERA_INIT_ONLY)
        self.assertEqual(self.status("相机 RAW8 单帧取帧", matrix), "NOT_VERIFIED")
        self.assertIn("相机 RAW8 单帧取帧未通过硬件矩阵", matrix["open_items"])

    def test_camera_requires_clean_metadata_only_frame(self):
        matrix = self.camera_matrix("camera-clean.txt", SYNTHETIC_CAMERA_CLEAN_FRAME)
        self.assertEqual(self.status("相机 RAW8 单帧取帧", matrix), "PASS")
        self.assertNotIn("相机 RAW8 单帧取帧未通过硬件矩阵", matrix["open_items"])

    def test_camera_capture_failure_is_not_verified(self):
        matrix = self.camera_matrix("camera-failure.txt", SYNTHETIC_CAMERA_FAILURE)
        self.assertEqual(self.status("相机 RAW8 单帧取帧", matrix), "FAIL")

    def test_camera_cleanup_error_blocks_pass(self):
        dirty = SYNTHETIC_CAMERA_CLEAN_FRAME.replace("cleanup_error=0", "cleanup_error=5")
        matrix = self.camera_matrix("camera-dirty.txt", dirty)
        self.assertEqual(self.status("相机 RAW8 单帧取帧", matrix), "FAIL")

    def test_camera_missing_metadata_does_not_count_as_clean(self):
        incomplete = "I (1200) Claw4V6: CAMERA_DIAGNOSTIC frame_capture=1 width=640 height=480\n"
        matrix = self.camera_matrix("camera-incomplete.txt", incomplete)
        self.assertEqual(matrix["signals"]["camera_frame_unqualified"], 1)
        self.assertEqual(self.status("相机 RAW8 单帧取帧", matrix), "FAIL")

    def test_camera_unqualified_retry_keeps_mixed_history_visible(self):
        incomplete = _write(
            self._dir.name, "camera-incomplete.txt",
            "I (1200) Claw4V6: CAMERA_DIAGNOSTIC frame_capture=1 width=640 height=480\n")
        good = _write(self._dir.name, "camera-good.txt", SYNTHETIC_CAMERA_CLEAN_FRAME)
        matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(good), hw_matrix.parse_boot_log(incomplete)], [],
            self.cand_path, CANDIDATE, {})
        self.assertEqual(self.status("相机 RAW8 单帧取帧", matrix), "PARTIAL")

    def test_camera_clean_and_failed_runs_are_reported_as_partial(self):
        good = _write(self._dir.name, "camera-good.txt", SYNTHETIC_CAMERA_CLEAN_FRAME)
        failed = _write(self._dir.name, "camera-failed.txt", SYNTHETIC_CAMERA_FAILURE)
        matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(good), hw_matrix.parse_boot_log(failed)], [],
            self.cand_path, CANDIDATE, {})
        self.assertEqual(self.status("相机 RAW8 单帧取帧", matrix), "PARTIAL")
        camera_row = next(r for r in matrix["rows"] if r["item"] == "相机 RAW8 单帧取帧")
        self.assertEqual(camera_row["source"], "camera-failed.txt")

    def test_display_fails_without_user_confirmation(self):
        confirmations = dict(display_visible=True, audio_loopback=True)
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
        """A captured abort is evidence of a defect, so the row must say FAIL --
        NOT_VERIFIED would understate it."""
        noisy = SYNTHETIC_BOOT + "abort() was called at PC 0x4ff1ed41 on core 0\n"
        path = _write(self._dir.name, "boot-assert.txt", noisy)
        matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            dict(display_visible=True, audio_loopback=True),
        )
        row = next(r for r in matrix["rows"] if r["item"] == "启动")
        self.assertEqual(row["status"], "FAIL")
        self.assertEqual(matrix["signals"]["assertions"], 1)

    def test_flash_row_is_not_downgraded_by_an_unrelated_boot_crash(self):
        """The flash capture is the only thing that can speak about flash writes."""
        noisy = SYNTHETIC_BOOT + "abort() was called at PC 0x4ff1ed41 on core 0\n"
        path = _write(self._dir.name, "boot-assert.txt", noisy)
        matrix = hw_matrix.build_matrix(
            [hw_matrix.parse_boot_log(path)],
            [hw_matrix.parse_flash_log(self.flash_path)],
            self.cand_path,
            CANDIDATE,
            dict(display_visible=True, audio_loopback=True),
        )
        row = next(r for r in matrix["rows"] if r["item"] == "Flash / PSRAM")
        self.assertEqual(row["status"], "PASS")

    def test_boot_attempts_are_counted(self):
        """A boot loop must be visible as a number, not only as a missing BOOT_READY."""
        looping = SYNTHETIC_BOOT + (
            "rst:0xc (SW_CPU_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)\n"
            "I (1530) main_task: Calling app_main()\n"
            "abort() was called at PC 0x4ff1ed41 on core 0\n")
        path = _write(self._dir.name, "boot-loop.txt", looping)
        s = hw_matrix.parse_boot_log(path)["signals"]
        self.assertEqual(s["boot_attempts"], 2)
        self.assertEqual(s["panic_resets"], 1)
        self.assertEqual(s["assertions"], 1)

    def test_render_markdown_contains_every_row(self):
        md = hw_matrix.render_markdown(self.matrix)
        for row in self.matrix["rows"]:
            self.assertIn(row["item"], md)
            self.assertIn(row["status"], md)
        self.assertIn("HEALTH 采样", md)

    def test_render_escapes_pipes_that_would_break_the_table(self):
        md = hw_matrix.render_markdown(self.matrix)
        # The AFE pipeline string contains literal '|'; it must not split a cell.
        # The signal table has three columns, i.e. four structural delimiters.
        line = next(l for l in md.splitlines() if l.startswith("| `afe_pipeline`"))
        unescaped = len(re.findall(r"(?<!\\)\|", line))
        self.assertEqual(unescaped, 4, line)

    def test_health_is_aggregated_across_captures(self):
        self.assertEqual(len(self.matrix["health_all"]), 2)
        self.assertTrue(all("log" in h for h in self.matrix["health_all"]))

    def test_render_warns_when_health_is_missing(self):
        matrix = dict(self.matrix)
        matrix["health_all"] = []
        md = hw_matrix.render_markdown(matrix)
        self.assertIn("证据缺口", md)


class ReviewRegressionTests(unittest.TestCase):
    def test_user_confirmations_default_to_unverified(self):
        self.assertFalse(hw_matrix.DEFAULT_USER_CONFIRMATIONS['display_visible'])
        self.assertFalse(hw_matrix.DEFAULT_USER_CONFIRMATIONS['audio_loopback'])

    def test_mixed_candidate_hashes_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = _write(directory, 'boot.txt', SYNTHETIC_BOOT +
                          'I (9) app_init: ELF file SHA256: abcdef123...\n')
            candidate = {'files': {'build/xiaozhi.elf': {'sha256': 'b' * 64}}}
            with self.assertRaisesRegex(ValueError, 'Candidate ELF mismatch'):
                hw_matrix.build_matrix([hw_matrix.parse_boot_log(path)], [],
                                       'candidate.json', candidate, {})

    def test_ansi_colored_log_is_parsed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = _write(directory, 'boot.txt',
                          '\x1b[0;32mI (101) V6M0: HEALTH free=100 psram=50 wake=1 taps=0\x1b[0m\n')
            capture = hw_matrix.parse_boot_log(path)
            self.assertEqual(capture['health'][0]['ts_ms'], 101)

    def test_blocked_boot_is_failure_without_abort(self):
        with tempfile.TemporaryDirectory() as directory:
            path = _write(directory, 'boot.txt', SYNTHETIC_BOOT +
                          'E (101) Claw4V6: BOOT_BLOCKED component=TCA9555\n')
            boot = hw_matrix.parse_boot_log(path)
            signals, sources = hw_matrix.aggregate_signals([boot])
            rows = hw_matrix.build_rows(signals, sources, {}, [boot], {})
            self.assertEqual(next(r['status'] for r in rows if r['item'] == '启动'), 'FAIL')

    def test_connection_message_without_ip_does_not_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            path = _write(directory, 'boot.txt', SYNTHETIC_BOOT +
                          'I (999) WifiBoard: Connected to WiFi: synthetic\n')
            boot = hw_matrix.parse_boot_log(path)
            signals, sources = hw_matrix.aggregate_signals([boot])
            rows = hw_matrix.build_rows(signals, sources, {}, [boot], {})
            self.assertEqual(next(r['status'] for r in rows if r['item'] == '网络 / IP'), 'NOT_VERIFIED')


if __name__ == "__main__":
    unittest.main()
