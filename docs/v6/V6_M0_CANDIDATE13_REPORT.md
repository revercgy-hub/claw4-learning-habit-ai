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

## Limits and next checks

- Camera behavior beyond successful sensor-stack initialization remains unverified. No image was captured or viewed.
- Power-key physical short/long events remain pending user action. Audio headroom and AEC/reference-channel behavior, actual network failure injection, SD hot-plug/repeated mounts, and long-duration stress remain open.
- M0 is not accepted; M1 remains BACKLOG.

Raw device data stays in the ignored `out/v6-device-private/` directory.
