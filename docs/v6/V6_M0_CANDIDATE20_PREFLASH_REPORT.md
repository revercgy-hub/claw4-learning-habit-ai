# Candidate20 S-03 pre-flash report

`S-03=BLOCKED_PRE_FLASH_PARTITION_MISMATCH`. Candidate20 was built from the reviewed source `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` in isolated `E:/v6/s3`. The build manifest is [`m0-candidate-20.json`](../../integration/v6/m0-candidate-20.json), frozen in commit `481c43803a9388ab400ad5022bc7137b5f28ee3c`. The app is 3,249,664 bytes, SHA-256 `b44397058ae7d714cb2acb7adb9c63d5ca01e214327d957705c9ac1e6c4ff4e0`; the ELF SHA-256 is `92562dfedc9ad11be32c3be34c15c8666b403ed5694e8a2aa04335c9264e8f4a`. Final build succeeded with the reviewed local Wi-Fi override. No Candidate20 image was flashed.

The live COM7 device identifies as ESP32-P4 v1.3, MAC `80:f1:b2:d2:ed:14`, 32 MiB flash, with Secure Boot and Flash Encryption disabled. Its partition table was read without writing and parsed with MD5 validation. It conflicts with the Candidate20 build table:

| Region | Live device | Candidate20 build |
| --- | --- | --- |
| `0x200000` app | `ota_0`, 9 MiB | `factory`, 14 MiB |
| model | `0x110000`, 956 KiB | `0x10f000`, 956 KiB |
| resources/assets | `resources` at `0xf00000`, 4 MiB | `assets` at `0x1000000`, 15 MiB |
| additional live partitions | `otadata`, `ota_1`, `emote`, `system`, `storage`, `coredump` | absent |

The live table's 4 KiB read has SHA-256 `8e5a526458f90a2b32a28ca0162611ca680e7ac8ca49f8336999ad85b529521a`; Candidate20's table image has SHA-256 `aeb3753bf4f27414820e29ea3282038357bffb6a4e65cb64c45bb263bd80f5de`. `flash_plan.py` passes against the *historical* 32 MiB backup, but that backup's partition table SHA-256 is `c254591397860d7c8e59e8c8c2762ed2efaec053b6ce5b4cfe1d69b19ecc4dc8` and does **not** match today's device. It cannot be treated as a current-layout recovery image. The live-layout comparison raised `ValueError: Candidate moves/resizes/retypes an existing partition`.

An application-only write to `0x200000` would leave the live OTA partition scheme while running an image built for the factory/model/assets layout. Changing the device table, bootloader, `ota_1`, or recovery scope is outside the S-03 contract. S-03 stopped before any Flash write. `DEVICE=NOT_VERIFIED`, `recovery_writeback=NOT_VERIFIED`, and `M0=IN_PROGRESS`; Candidate17/18/19 results were not inherited.

The private, ignored `out/s03` directory retains `build-02.txt` (SHA-256 `7ec38d4b1ba0904df2cbbbcc35f5b29306902dd63da7b2f79a671b89da441507`), the read-only identity/security/partition logs, and the 4 KiB partition read. Their individual hashes are recorded in [`m0-candidate-20-preflight.json`](../../integration/v6/m0-candidate-20-preflight.json). Commands run: `stage_device.py --upstream E:/workbuddy/claw4-v6/vendor/xiaozhi-esp32 --destination E:/v6/s3`; `build_device.py --source E:/v6/s3 --idf E:/workbuddy/claw4-v6/vendor/esp-idf --tools E:/workbuddy/claw4-v6/toolchains/idf61` with the pinned IDF Python 3.12; `freeze_candidate.py --source E:/v6/s3 --candidate claw4-learning-v6-m0.20`; read-only `esptool flash_id`, `get_security_info`, and `read-flash 0x9000 0x1000`. The first build invocation with system Python 3.14 stopped before compilation for missing `rich_click`; the pinned environment completed the build. No device matrix, recovery writeback, or physical stimulus was run.

The next decision belongs to Astra and the user under the partition/data-risk stop condition. This report does not approve a partition update or a different Flash command.
