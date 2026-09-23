# V6 M0 Candidate 18 — bounded one-frame camera diagnostic

Date: 2026-09-23. Candidate18 builds on Candidate17 and extends the existing boot-time sensor-presence probe to dequeue exactly one frame per boot from the Claw4 MIPI CSI camera. The frame remains in the video driver's temporary MMAP buffers; application code logs only dimensions, pixel format, byte count, and cleanup status. No image is written to storage, displayed, copied to a persistent buffer, or uploaded.

## Implementation

- Opens the existing `/dev/video0` MIPI CSI device and verifies capture/streaming capability.
- Selects the smallest advertised `V4L2_PIX_FMT_SBGGR8` frame size, enforces a 2 MiB per-buffer limit, requests at most two driver-owned buffers, and sets the driver's dequeue timeout to three seconds.
- Dequeues one valid frame, then stops streaming, unmaps buffers, releases the V4L2 allocation, closes the device, deinitializes `esp_video`, and powers the camera rail down. Every failure is reported with a stage and error code; the diagnostic does not abort normal boot.
- The log reports `saved=0 uploaded=0`; it contains no pixel data or image hash. This per-boot diagnostic is not a camera product feature.

## Build evidence

- Candidate manifest: [`integration/v6/m0-candidate-18.json`](../../integration/v6/m0-candidate-18.json).
- App image SHA256: `88b4fc2162f79c957ac463dad60028836c016336df400554f54e765745d0b1db` (3,247,904 bytes); ELF SHA256: `945adab02d50177357c5b87cdb9fc28bbddbb332ba6f564ceb96333b7f7112ce`.
- ESP-IDF 6.1 incremental reconfigure/build completed successfully, including the `claw4_board.cc` compile and final link. The app partition check reported 78% free. The private build log is `out/v6-device-private/candidate18-build-01.txt`, SHA256 `865c2876ba64ff19ea8fbe12b0de62471792b6280595cb4bcaea145588d0fde9`. `py -3.14 -m unittest discover -s tools/v6 -p "test_*.py"` passed all 63 tests; `git diff --check` passed.

## Device gate

Candidate18 has **not** been flashed. No frame has been captured from the physical device, so sensor-stack initialization is still the only camera behavior verified on hardware. App-only flash and a one-frame diagnostic run remain pending; a successful run must show `frame_capture=1` and `cleanup_error=0`, with no panic or boot block. If capture fails, retain only the redacted UART diagnostic and report its stage/error. This report does not change the AEC result: AEC remains **NOT PASS**, and the Candidate17 controlled audio interaction remains pending.
