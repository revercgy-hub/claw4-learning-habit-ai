# V6 M0 Candidate20 — A-02 Gate

Date: 2026-09-24. Decision: **`M0=CHANGES_REQUIRED`**. M1 remains `BACKLOG`.

Candidate ID is `claw4-learning-v6-m0.20`. R-01 independently accepted CODE/HOST at reviewed source `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` after 96 Host tests passed with 0 failures. [Build manifest](../../integration/v6/m0-candidate-20.json) binds that source SHA, pinned upstream/IDF, overlay/config inputs, app SHA-256 `b44397058ae7d714cb2acb7adb9c63d5ca01e214327d957705c9ac1e6c4ff4e0` and ELF SHA-256 `92562dfedc9ad11be32c3be34c15c8666b403ed5694e8a2aa04335c9264e8f4a`. BUILD passed in isolated `E:/v6/s3`; Astra independently recalculated app, ELF and build-log hashes.

S-03 stopped before Flash write. The [preflight evidence](../../integration/v6/m0-candidate-20-preflight.json) shows COM7's live `ota_0` is 9 MiB at `0x200000`, with `ota_1` present. Candidate20 was built for a 14 MiB `factory` app at the same address; model and assets offsets also differ. The live partition-table read SHA-256 is `8e5a526458f90a2b32a28ca0162611ca680e7ac8ca49f8336999ad85b529521a`, different from Candidate20's partition image. The historical 32 MiB backup's partition table also differs from today's device. [S-03 report](V6_M0_CANDIDATE20_PREFLASH_REPORT.md) retains commands, source/artefact hashes and private-log index. No Candidate20 DEVICE log or validated session exists; no previous Candidate DEVICE PASS transfers here.

| Evidence | A-02 finding |
| --- | --- |
| CODE | R-01 PASS at the reviewed source SHA; AEC effectiveness is not a CODE claim. |
| HOST | 96/96 unit tests passed, 0 failures/errors; Host does not establish device behavior. |
| BUILD | Candidate20 build, frozen inputs, manifest and full app/ELF hashes verified. |
| DEVICE | `NOT_VERIFIED`: no app flash, readback or device matrix; no Candidate20 log/session binding. |
| Recovery | `NOT_VERIFIED`: no write-back; historical backup layout differs from live device. |

Remaining M0 gaps:

1. Decide and review a device-layout and recovery strategy based on the **current** partition table and a current, verified backup. Any partition/bootloader/`ota_1` change or broader Flash/recovery operation needs a separate explicit decision before execution. Candidate20 must not be flashed under the present contract.
2. Freeze a candidate whose build inputs and permitted write region match the chosen device layout. Bind its source SHA, manifest, complete image/ELF hashes, readback and log sessions to that one ID.
3. Execute the same candidate's required DEVICE matrix: controlled Audio/AEC and near-end wake, Network failure/recovery, Camera capture/cleanup, SD, power key, real power-cycle cold boot with I2C/TCA9555, stability and fault injection. Record unexecuted items `NOT_VERIFIED` and failures as failures.
4. Obtain recovery write-back evidence within a separately reviewed safe scope. Until all required evidence belongs to the same valid candidate, M0 cannot PASS and M1 cannot start.
