# V6 M0 Candidate 12 — SD Slot 0 mount diagnostic

Date: 2026-09-23. Candidate12 adds a one-shot, nonfatal SDMMC Slot 0 mount diagnostic after ESP-Hosted reports the first Wi-Fi scanning event. Candidate11 power-key diagnostics remain enabled. Camera remains powered down.

## Implementation

- Added the Claw4 SD Slot 0 GPIO map (CLK 43, CMD 44, D0-D3 39-42) and LDO channel 4 from the read-only Metalio board reference. That reference explicitly says to verify the GPIO assignments against the hardware schematic; the mount result is device evidence, not a schematic audit.
- Keeps the active-low SD rail off during initial board setup; the diagnostic task enables it only after C5/ESP-Hosted has entered scanning, then initializes Slot 0 at 20 MHz.
- Uses `format_if_mount_failed=false`, does not enumerate directories or open files, and logs only card geometry/capacity. A failure is nonfatal and does not block the boot loop.
- Mounting is read-write at the VFS layer, but this diagnostic contains no file or block write path. It does not claim read-only filesystem semantics.

## Build and software checks

- ESP-IDF 6.1 / ESP32-P4 incremental configure, compile, link and image generation passed from the locked staged tree `E:/v6/s1`.
- App image: 3,092,608 bytes, SHA256 `0c5ce7dabf6e9c01fe167714e09507ce37791fdda476843339eb32f88a7a52b9`.
- `py -3.14 -m unittest discover -s tools/v6 -p "test_*.py"`: 62 passed.
- `test_network_station.py`: 19 production-method host scenarios passed.
- `test_board_algorithms.cc`: compiled with C++17 `-Wall -Wextra -Werror` and passed.
- Candidate manifest: [`integration/v6/m0-candidate-12.json`](../../integration/v6/m0-candidate-12.json).

## Device evidence

- Device identity remained ESP32-P4 rev 1.3, MAC `80:f1:b2:d2:ed:14`; the full pre-V6 backup hash remained `b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413`.
- The flash plan was validated, then narrowed to the factory application at `0x200000`. No bootloader, partition table, speech-model, assets, NVS, or other partition was written.
- esptool verified the write. Reading back the complete 3,092,608-byte application region produced the same SHA256 as the build image.
- A 60-second capture reported `Assets applied=1`, `BOOT_READY`, normal health samples, and no panic, abort, or `BOOT_BLOCKED` marker.
- The actual board logged `SD_DIAGNOSTIC mounted blocks=7864320 sector_bytes=512 capacity_bytes=4026531840`. This confirms a 4 GiB-class card initialized and mounted through Slot 0. No directory listing or file access occurred; auto-format is disabled.

## Limits and next checks

- This verifies one boot-time mount with the attached card; hot-plug, repeated mount/unmount, sustained I/O, and card removal recovery are not tested.
- The board reference itself flags the GPIO map for schematic confirmation. No camera path was added.
- Candidate11's physical power-key short/long recognition is still pending. Audio headroom and AEC/reference-channel quality remain unverified. M0 is not accepted; M1 remains BACKLOG.

Raw device data stays in the ignored `out/v6-device-private/` directory.
