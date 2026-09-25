# S-04 M1 Voice Preflight preparation

Status: **BLOCKED_DATA_LOSS_RISK**. No M1 Candidate was frozen, built, flashed, or device-tested. R-02 and S-05 must not use this work as a ready-to-flash release. M0 remains `CHANGES_REQUIRED`; Candidate20 AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`.

## Blocking source finding

The pinned XiaoZhi v2.5.0 upstream `main/main.cc:17-21` initializes NVS and, when `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, calls `nvs_flash_erase()` before trying again. The S-04 stage contains those exact lines unchanged. A Voice Preflight boot could therefore erase live NVS data upon either error. Editing that upstream entry point or changing NVS policy is outside this task's file whitelist. Astra stopped candidate progression on this data-loss risk before any device operation. A separately reviewed, bounded decision and implementation are required before building a flashable Candidate.

## Live layout and proposed build-only inputs

S-03's private read-only `device-partition-before.bin` is 4,096 bytes, SHA-256 `8e5a526458f90a2b32a28ca0162611ca680e7ac8ca49f8336999ad85b529521a`. ESP-IDF `gen_esp32part.py` decoded `otadata` at `0x10e000/8K`, `phy_init` at `0x110000/4K`, `model` at `0x111000/956K`, `ota_0` at `0x200000/9M`, `ota_1` at `0xb00000/4M`, and `resources` at `0xf00000/4M`, plus the remaining partitions in `integration/v6/board/claw4-live-m1.csv`. Generating the binary from that CSV gave 3,072 bytes exactly equal to the first 3,072 bytes of the live table; the remaining live 1,024 bytes were not written. The existing `claw4-v6.csv` was not changed. This is build configuration, **not** authorization or a plan to write a partition table.

An additive M1 board build entry selects the live CSV, disables the M0 diagnostic entry point, retains device AEC, and points `CONFIG_OTA_URL` at plain HTTP `http://192.168.3.100:7443/xiaozhi/ota/`. The deployed OTA response supplies `ws://192.168.3.100:7444/xiaozhi/v1/`; WebSocket URL is an upstream persisted setting loaded from that response, not a separate build flag. Upstream `Application`, `AudioService`, AFE and protocol remain the single Voice owner. No Learning UI or M1.5 router was added.

The proposed board adapter feeds accepted post-volume TX samples into the AFE reference slot even with M0 diagnostics disabled, with bounded queue/session reset and periodic queue telemetry. Candidate20's physical RX reference slot was silent. The zero-frame software offset remains uncalibrated; this change does **not** prove effective acoustic cancellation or safe near-end speech. Upstream playback and Wake lifecycle remain owned by XiaoZhi. For a later hardware Preflight, record whether TTS is recaptured, whether self-dialogue starts, near-end speech during playback, Wake rearm, crash/WDT/reboot, and heap/PSRAM trend.

Upstream `main/assets.cc:20,43-47` looks specifically for an `assets` label while the live table contains `resources`. `Application::CheckAssetsVersion()` returns when the assets partition is invalid (`main/application.cc:390-394`). The AFE fallback independently initializes models by `esp_srmodel_init("model")` (`main/audio/engines/afe_audio_engine.cc:65-68`), matching the live label. Thus the label mismatch is an assets/theme gap and is not by itself evidence that Voice startup must fail; only a device run can confirm startup and Wake.

## Offline evidence and limits

- Source base: `5849e6e6ac3c3bffb3a7a1ad6bfcf5921fa08a94`. Pinned upstream XiaoZhi SHA `ac6deed3d8e75348475364bf40ad953c6cd48054`; ESP-IDF SHA `fff9895c82d744c7237be8847347bdd1b07c6643`.
- `py -3.14 -B tools/v6/stage_device.py --upstream E:/workbuddy/claw4-v6/vendor/xiaozhi-esp32 --destination E:/v6/m1-s04 --variant m1`: PASS. Private stage manifest SHA-256 `c5822f76bd49614dbdbe50f1802c0f0c8a9dc870a74839779b297e9acdcd109b` is a staging trace, not a frozen Candidate manifest.
- `E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe -std=c++17 -Wall -Wextra -Werror tools/v6/test_board_algorithms.cc` with that compiler's `bin` in PATH, followed by execution: PASS.
- `py -3.14 -B -m unittest discover -s tools/v6 -p 'test_*.py'`: 109 tests, 0 failures/errors. Private log `E:/v6/m1-s04-host-tests.log` SHA-256 `16c538142a9481ffa23b716490ac7949051d23d026349afcbc2fcb0a1ece9280`.
- Pinned IDF 6.1 Python 3.12 build was started in private `E:/v6/m1-s04` and interrupted by Astra's data-loss stop while managed components were resolving (last log entry `lvgl/lvgl (9.5.0)`). Private `E:/v6/m1-s04-build.log` SHA-256 `778262f575f2460c6988265ac9116ccd844f151dee50ad86445f6499f10839af`. No `sdkconfig`, app binary or ELF was produced. The earlier Python 3.14 attempt failed before configuration because the pinned IDF environment is Python 3.12; its log was overwritten by the pinned attempt.
- No source/image/ELF identity or build output is frozen, no preflight candidate JSON was created, and no DEVICE PASS or M1 PASS is claimed. No COM7, flash, NVS, C5, bootloader, `ota_1`, partition table, or shared `E:/v6/s1` operation occurred.

Next decision: address the upstream automatic NVS erase path under a separately reviewed scope, then resume a clean pinned build, bind exact input/app/ELF hashes and a unique Candidate manifest, seek independent R-02, and only after PASS hand the device to the sole S-05 owner for 2-3 Preflight rounds. A stable Preflight would still not be M1 PASS; formal 20 rounds would start again at round 1 after any firmware change.
