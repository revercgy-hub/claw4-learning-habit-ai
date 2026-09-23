# V6 M0 Candidate 13 — metadata-only camera sensor probe

Date: 2026-09-23. Candidate13 extends Candidate12 with a one-shot camera sensor-stack probe after the SD mount attempt. It does not open a video device, start a stream, dequeue frames, save images, or send images. The camera rail returns to its powered-down state after the probe.

## Implementation

- Uses the Claw4 camera PWDN expander line (TCA9555 P0_2, active-low) and reference XCLK GPIO32 at 24 MHz.
- Initializes `esp_video` on the existing SCCB/I2C bus after the SD diagnostic finishes, logs initialization status, deinitializes it, stops/frees XCLK, and powers the camera rail down.
- Camera power remains off during ordinary boot until the one-shot diagnostic runs. No camera application, live preview, streaming, image capture, file write, or upload path was added.
- This proves sensor-stack initialization on this unit, not image quality, format negotiation, focus, or frame capture.

## Build and software checks

- ESP-IDF 6.1 / ESP32-P4 incremental configure, compile, link, and image generation passed from staged tree `E:/v6/s1`.
- App image: 3,244,128 bytes, SHA256 `c23929ace311dc7bbf7006cccf2489fe5d1ced316dbcf3d619ddd111382e69a7`.
- `py -3.14 -m unittest discover -s tools/v6 -p "test_*.py"`: 62 passed.
- `test_network_station.py`: 19 production-method host scenarios passed.
- `test_board_algorithms.cc`: compiled with C++17 `-Wall -Wextra -Werror` and passed.
- Candidate manifest: [`integration/v6/m0-candidate-13.json`](../../integration/v6/m0-candidate-13.json).

## Device evidence

- Device identity remained ESP32-P4 rev 1.3, MAC `80:f1:b2:d2:ed:14`; the full pre-V6 backup hash remained `b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413`.
- The validated write plan was narrowed to the factory application at `0x200000`; no bootloader, partition table, speech-model, assets, NVS, or other partition was written.
- esptool verified the app write. A full 3,244,128-byte app-region readback matched the build image SHA256 exactly.
- A 60-second capture reported `Assets applied=1`, `BOOT_READY`, the 4 GiB-class SD card mounted, and `CAMERA_DIAGNOSTIC sensor_stack_initialized=1 frame_capture=0`. It showed no panic, abort, or `BOOT_BLOCKED` marker.
- Five health samples ranged from 26,923,451 to 26,924,475 bytes of internal heap and 26,640,256 to 26,641,280 bytes of PSRAM. The final readings were about 152 KB internal / 144 KB PSRAM below Candidate12's steady readings, with no further downward trend beyond 1 KiB sampling variation in this single-minute run. The delta is recorded for follow-up and not attributed to a specific allocator.
- A second 60-second capture without reset, at roughly 250–300 seconds of app uptime, held at 26,923,471 bytes internal / 26,640,276 bytes PSRAM across all six health samples. `taps=0`; no power-key or audio interaction occurred during that window.
- A later 60-second USB-reset boot repeated `BOOT_READY`, SD mount, and sensor-stack initialization. One physical touch triggered the bounded 3-second record/playback test; playback drained (`LOCAL_REFERENCE_PROBE_END drained=1`). The user confirmed they spoke only while recording and the room was quiet during playback. The VAD nevertheless reported two speaking transitions during playback.
- In that capture, both input channels produced 852,000 samples across 54 windows with no I2S read failures or clipping. Channel 0 reached RMS 340 and peak 17,323. Channel 1 remained RMS 0 / peak 0 / raw peak 1 in all 54 windows, including 47,104 samples counted during nonzero I2S write calls; the overlap counter is not a physical DAC timing measurement. With `CONFIG_USE_DEVICE_AEC=y`, the silent reference and VAD activity during quiet playback reproduce an AEC/reference-path failure. The audible record/playback loopback remains a pass, but it does not make AEC pass.
- The same boot captured physical `POWER_KEY_SHORT` and `POWER_KEY_LONG` events. These confirm press recognition only; the diagnostic intentionally performs no power action. Wi-Fi progressed through scanning and connection retries to `NETWORK_EVENT=2` (connected/IP acquired). No panic, abort, or `BOOT_BLOCKED` marker appeared.
- A 60-second continuation on that boot tested the wake phrase. The user reported four quiet utterances of “你好小智”; the device logged two `WAKE_DETECTED` events, each followed by `WAKE_REARM armed=1`. The user accepted this as functional wake-word evidence. It demonstrates repeat detection and recovery, not robust sensitivity or a calibrated recognition rate.
- The SHA-bound audio summary covers the two latter captures under ELF SHA256 `065f85955c5fabed4f2d595b473a9e62287a2626566f79d2a5fefa6a70064d59`. `candidate13-coldboot-repeat-01.txt` SHA256 is `da9d45e4af680564b4b20c3fd35a2077ce59664b68ac48aaf23f616aecab20a9`; `candidate13-wakeword-01.txt` SHA256 is `878b6982510be9ac8946f5cf83b4f4304c9d787df96bcf35bab534b7ef831d2e`. Across these two 60-second windows, `audio_evidence.py` reports 10 health samples, two wake events, two rearm lines, 113 windows per channel, zero clipping/read failures, and channel 1 still all-zero. The captures are separate windows, not a continuous soak.

## Limits and next checks

- Camera behavior beyond successful sensor-stack initialization remains unverified. No image was captured or viewed.
- Power-key short/long detection is verified; product power behavior is not implemented. Wake word is functional at low volume but only two of four requested utterances were detected. Normal-volume headroom remains only partially characterized.
- AEC/reference-channel behavior is a reproduced defect and blocks M0 closure. The Claw4 RX reference slot is silent during playback; the M0 harness uses the configured device AEC. Do not proceed to concurrent TTS/listen claims or M1 until the reference source/timing is corrected and re-tested.
- Next engineering experiment: feed a bounded, post-volume copy of I2S TX PCM as the AFE `R` channel, with delay tied to measured playback/input timing. Check both VAD suppression during quiet playback and wake-word retention for near-end speech. This is a software-reference hypothesis, not yet a fix or a claim that the hardware module cannot provide a reference.
- Actual Wi-Fi synchronous failure injection, SD hot-plug/repeated mounts, and long-duration stress remain open. Recovery writeback remains outside this candidate.
- M0 is not accepted; M1 remains BACKLOG.

Raw device data stays in the ignored `out/v6-device-private/` directory.
