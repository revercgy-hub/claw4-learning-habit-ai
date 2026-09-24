# V6 M0 Candidate20 — A-02 Gate

Date: 2026-09-24. Decision: **`M0=CHANGES_REQUIRED`**. Candidate20 has partial DEVICE evidence; M0 is not passed.

Candidate ID is `claw4-learning-v6-m0.20`. R-01 accepted CODE/HOST at reviewed source `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` after 96 Host tests passed with 0 failures. [Build manifest](../../integration/v6/m0-candidate-20.json) binds that source SHA, pinned upstream/IDF, overlay/config inputs, app SHA-256 `b44397058ae7d714cb2acb7adb9c63d5ca01e214327d957705c9ac1e6c4ff4e0` and ELF SHA-256 `92562dfedc9ad11be32c3be34c15c8666b403ed5694e8a2aa04335c9264e8f4a`. BUILD passed in isolated `E:/v6/s3`; Astra independently recalculated app, ELF and build-log hashes.

## Evidence review

| Evidence | A-02 finding |
| --- | --- |
| CODE | `PASS` at the R-01 reviewed source SHA; AEC effectiveness is not a CODE claim. |
| HOST | `PASS`: 96/96 tests, 0 failures/errors. Host does not establish device behavior. |
| BUILD | `PASS`: frozen inputs, manifest, full app/ELF hashes and build log verified. |
| DEVICE | `PARTIAL / CHANGES_REQUIRED`: app-only write to live `ota_0` and exact readback passed. Bound session observed C5, saved-network connectivity, SD and local record/playback. Camera frame open failed (`error=2`, cleanup `0`); assets reported no partition and `applied=0`. Controlled AEC/wake and several required device checks remain unverified. |
| Recovery | `NOT_VERIFIED`: no recovery write-back. The historical full backup partition table differs from the live table. |

S-03 wrote only the app bytes at `0x200000` in the live 9 MiB `ota_0` after taking a full `ota_0` backup. The app readback was byte-equal to the Candidate20 application image. No bootloader, partition table, `ota_1`, NVS, model/resources, C5, or eFuse region was written. The candidate's build partition layout still differs from the live table (factory versus `ota_0`; assets/model offsets differ from resources/model). This partial app operation does not resolve the layout or recovery mismatch. Exact build, flash, readback, session and log hashes are recorded in the [S-03 device report](V6_M0_CANDIDATE20_DEVICE_REPORT.md) and [device evidence JSON](../../integration/v6/m0-candidate-20-device-evidence.json).

The first bound Candidate20 session used a USB reset, not a cold boot. It included C5, connectivity on the previously saved network, SD mount and local playback observations. The later identity-anchored USB-reset capture showed `505` association, IP `192.168.3.49`, and network events `0 → 1 → 2`; however, the strict session validator rejected that capture due to a 6 ms timestamp rollback, so it is retained as a raw same-boot observation and not a generated matrix PASS. The user chose to skip the `505` AP outage/recovery stimulus. That item is `SKIPPED_BY_USER / NOT_VERIFIED`, never PASS.

Additional bounded findings:

- Audio reference telemetry and playback VAD do not establish AEC quality. The user's report of audible feedback at low speaking volume is retained, but the speech stimulus is not causally bound to a valid capture. AEC and wakeword remain `NOT_VERIFIED`.
- The user physically pressed short and long power-key actions and observed no device-state change. The available fragment is not bound to the validated Candidate20 session, so this is a failure signal for follow-up, not a formal matrix result.
- A genuine power-cycle cold boot, I2C/TCA9555 result, long stability and fault injection remain `NOT_VERIFIED`.
- Candidate17/18/19 DEVICE results are not transferred to Candidate20.

## Remaining M0 gaps

1. Resolve the live-layout/build-layout and verified-recovery mismatch before another device candidate. Any partition, bootloader, `ota_1`, or broader recovery operation still needs a separately scoped user decision.
2. Produce a layout-compatible candidate and bind its source, build inputs, manifest, image, readback and logs to one identity.
3. Close the required DEVICE matrix on one candidate: controlled audio/AEC and near-end wake; network failure/recovery (currently skipped); Camera frame capture/cleanup; assets/resources; SD; power-key action; real cold boot with I2C/TCA9555; stability; and fault injection.
4. Obtain recovery evidence within a separately reviewed safe scope.

## User-directed next-stage development exception

On 2026-09-24 the user explicitly directed us to skip the AP outage/recovery stimulus as unimportant and start the next-stage development task. This authorizes **M1 development preparation and host/tool work only** while `M0=CHANGES_REQUIRED`; it does not change any evidence result, declare M0 PASS, or claim that M1's 20-round acceptance has run. No NAS service deployment, provider credential setup, child-data upload, device reflash, or expanded Flash/recovery operation is included. M1 device validation remains unstarted pending its environment and explicit device-operation scope.
