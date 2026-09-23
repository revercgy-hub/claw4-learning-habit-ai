# V6 M0 Candidate 11 — power-key diagnostic

Date: 2026-09-23. Candidate 11 adds observation-only power-key diagnostics to the M0 local hardware harness. It does not issue shutdown, standby, wake, or power-pulse commands.

## Implementation

- Samples TCA9555 P0_5 as an active-low input, with 50 ms debounce and a 1.5 s long-press threshold.
- Ignores a key held during startup until it is released; logs `POWER_KEY_SHORT` or `POWER_KEY_LONG` after a debounced event.
- Reads are serialized with existing expander writes. Input read errors are logged and skipped; they do not block boot or reset the shared I2C bus.
- The pure button state machine is covered for startup-held, bounce, short press, and one-shot long press by `tools/v6/test_board_algorithms.cc`.

## Build and software checks

- ESP-IDF 6.1 / ESP32-P4 incremental configure, compile, link and image generation passed, using the already verified candidate10 dependency cache and lock.
- App image: 3,052,480 bytes, SHA256 `d3f84a4f3d1f375820b45734494f93be6f46f1d5ef1229633572f57e8e9c09ee`.
- `g++ -std=c++17 -Wall -Wextra -Werror tools/v6/test_board_algorithms.cc`: passed.
- `py -3.14 -m unittest discover -s tools/v6 -p "test_*.py"`: 62 passed.
- `test_network_station.py`: 19 production-method host scenarios passed.
- Candidate manifest: [`integration/v6/m0-candidate-11.json`](../../integration/v6/m0-candidate-11.json).

## Device evidence

- Full 32 MiB pre-V6 backup hash remained `b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413`; flash identity was ESP32-P4 rev 1.3, 32 MiB, MAC `80:f1:b2:d2:ed:14`.
- The validated write set was narrowed to the factory application at `0x200000`; no bootloader, partition table, speech-model, assets, NVS, or other partition was written.
- esptool wrote 3,052,480 bytes and verified its hash. A full app-region readback matched the build image byte-for-byte and SHA256.
- A 60-second boot capture reported `POWER_KEY_DIAGNOSTIC armed_after_boot_release=1`, `Assets applied=1`, and `BOOT_READY`; it showed no panic, abort, or `BOOT_BLOCKED` marker.
- Network reached connected state during this run. The 60-second capture contained no power-key press markers; physical short/long event verification is still pending the operator action.

## Limits and next checks

- This proves that the input diagnostic initializes and does not block startup; it does not yet prove physical short/long recognition on the connected unit.
- No product power-key behavior is implemented. SD and Camera remain out of this candidate.
- AEC/reference-channel behavior and normal-volume headroom remain unverified. M0 is not accepted, and M1 remains blocked by the M0 gate.

Raw device data stays in the ignored `out/v6-device-private/` directory.
