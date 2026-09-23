# V6 M0 AEC playback-reference experiments

Date: 2026-09-23. This report supersedes the “next experiment” proposal at the end of [`V6_M0_CANDIDATE13_REPORT.md`](V6_M0_CANDIDATE13_REPORT.md). Candidates 14 and 15 tested a bounded software copy of post-volume TX PCM as the AFE reference channel. They establish that software reference samples can reach AFE input channel 1; they do **not** establish that AEC is effective.

## Change under test

The Claw4 physical RX channel 1 remains silent. Under the M0 diagnostic build, accepted stereo I2S TX frames are copied into a bounded 8,192-sample queue; the left-channel PCM is decoded and substituted for channel 1 as `Read()` prepares the AFE input. A fixed circular buffer optionally delays that copy, bounded to 2,048 frames. Queue overflow is counted and does not overwrite older samples. This instrumentation is compiled only when `CONFIG_CLAW4_M0_DIAGNOSTICS` is enabled. It does not modify vendor sources, partition data, bootloader, or NVS.

The queue and delay behavior have host coverage in `tools/v6/test_board_algorithms.cc`. Candidate 14 used 1,440 frames (90 ms) of added delay. Candidate 15 used zero added delay. These values are experiment settings, not measured acoustic delays.

## Candidate 14 — 90 ms added delay

- Manifest: [`integration/v6/m0-candidate-14.json`](../../integration/v6/m0-candidate-14.json).
- App image SHA256: `3ab05b8f88a4046ca67fb051d1809520a3390100f4f650a26f9251f857632d07` (3,244,944 bytes); ELF SHA256: `205d0bb85772e0745e510f32742aa0725ac69b71916c0d9dc10ae013b42c3461`.
- The app was written only to the factory app region at `0x200000`; esptool reported `Hash of data verified`. No full-region readback was performed for this candidate.
- The captured boot reported `BOOT_READY`; the log records two touch-triggered record/playback probes, zero reference-queue drops, and software-reference statistics on channel 1 (maximum RMS 246, peak 5,902). Both playback probes drained in firmware. User reported hearing both playbacks and that the playback periods were quiet; only one recording contained speech, but the user did not identify which recording.
- The first playback window had no VAD speaking transition. The second had three speaking-on transitions. Because the two recordings were not content-matched and their speech/no-speech assignment is unknown, this is not a controlled echo-suppression comparison.
- UART capture SHA256: `c90625e2a98bd8edc7d0b0f96d2cd531ba5b21b64ddf72fd6ca3873b0701f5ca`; private capture is retained under ignored `out/v6-device-private/`.

## Candidate 15 — zero added delay

- Manifest: [`integration/v6/m0-candidate-15.json`](../../integration/v6/m0-candidate-15.json).
- App image SHA256: `352132915e314783635f7a8ca006aba7784564098cebf14bcdc6a033be905d8f` (3,244,944 bytes); ELF SHA256: `a29fb055d5681e25f95fba3845328dad8c4c2fe7315a8d9575262cf5568d987c`.
- The app was written only to the factory app region at `0x200000`; esptool reported `Hash of data verified`. The boot capture contains the matching ELF anchor, `BOOT_READY`, and no panic, abort, or `BOOT_BLOCKED` marker.
- The capture records two touch-triggered record/playback probes, zero reference-queue drops, and injected software-reference samples on channel 1 (maximum RMS 128, peak 1,353 across the capture). Both probes drained in firmware. User reported speaking during both recording periods and remaining quiet during both playback periods; the user did not confirm audibility for this Candidate 15 run.
- The first playback window had two VAD speaking-on transitions; the second had none. This order-dependent result does not demonstrate AEC improvement: the playback content was not documented as identical, there were only two trials, and no calibrated acoustic measurement was taken.
- Main UART capture SHA256: `1fae9be304126a4769aed3a3c0d07621cb7e12e206a2fae0709b0f9a8dc29b5d`. A subsequent 60-second continuation with no user touches is only an idle baseline, not a playback repeat; its SHA256 is `6b676c032259184a3e35e911db78404b7789df79cf15c1e8b3c3247f03bbcd93`.

## Candidate 16 — phase-tagged diagnostics

Candidate16 retains Candidate15's zero-added-delay software reference and adds a monotonically increasing probe ID, recording/playback phase tags on VAD callback lines, and a per-probe playback VAD-onset total at probe end. In VAD lines, `phase=0` means idle, `phase=1` recording, and `phase=2` playback. This makes the next controlled playback test easier to attribute without storing raw audio.

- App image SHA256: `504c39a461049579e5846f7e0411c27623703a8b1089265835933af724de5a5b` (3,245,424 bytes); ELF SHA256: `90b2dc42647cf12e6c873a520774a1966184b99950003d7022e780f25ed4b1f2`.
- Only the factory app region at `0x200000` was written; esptool reported `Hash of data verified`. The 60-second boot capture contains the matching ELF prefix and `BOOT_READY`, with no panic, abort, or `BOOT_BLOCKED` marker.
- No touch occurred in this capture, so it verifies boot stability only. Playback and AEC have not yet been retested on Candidate16.
- Boot capture SHA256: `dd0dc8b782092c0b0d120be62fdc041e4e296d4adc3092e37a9b7f5480477c42`; raw capture stays in the ignored private directory.

## Candidate 17 — local WakeNet during playback

Candidate16 disabled WakeNet for the whole recording/playback probe. Candidate17 now re-enables the local detector at playback start while voice processing and the software reference are active, and attaches the probe ID/phase to any wake callback. Detected audio still stays local; the protocol and OTA loop are not started.

- App image SHA256: `5f78d7f7b1e0412cb2cae1c38971b3c14c72b4d325f4dd986c00f933c43ca53c` (3,245,616 bytes); ELF SHA256: `6735476b1504efa21b257dba018e3e4c626c58b39fec4adf4cddd3a5d3e50e4`.
- Only the factory app region at `0x200000` was written; esptool reported `Hash of data verified`. A 60-second cold-boot capture contains the matching ELF prefix and `BOOT_READY`, with no panic, abort, or `BOOT_BLOCKED` marker.
- The boot window had no touch, so local WakeNet during playback and AEC are not yet tested on Candidate17.
- Boot capture SHA256: `2e8d9397fb600b7176567160e23d0d9515cd5862a66b88e763101c0fffd35692`.

## Conclusion and next gate

The software reference substitution is observable on AFE channel 1, and the bounded queue drained without reported drops during both candidates. The experiment does not show that AEC cancels speaker echo. VAD activity remained in quiet playback windows, and Candidate 14/15 outcomes differ without controlled, paired audio. The hardware reference channel remains physically silent; `tx_overlap_n` is a software write-call overlap statistic, not proof of acoustic or DAC alignment.

**AEC is NOT PASS; M0 remains IN_PROGRESS and M1 remains BACKLOG.** Do not claim simultaneous playback/listening or wake-word performance during playback. Candidate17 is ready for a controlled test: first use the same short spoken phrase and volume for several paired trials, staying quiet during playback; then repeat with one near-end “你好小智” during playback. Candidate17 logs each probe ID, recording/playback phase, VAD transitions, playback VAD-onset total, and wake detections during playback; correlate these with injected reference level, queue drops, and the user's audibility report. Until those results are available, do not tune delay based on Candidates 14/15 alone. AEC product integration stays behind this gate.

Other M0 gaps remain: synchronous network failure injection, longer stability coverage, actual camera frame capture, and wake-word sensitivity/headroom. Candidate 13's sensor initialization and wake tests do not close those items. Raw UART captures remain in ignored `out/v6-device-private/`; no raw audio is stored in the repository.
