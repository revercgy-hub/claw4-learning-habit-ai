# M1 Voice operator runbook

This runbook supports evidence collection; it does not change Voice architecture. Voice Core remains the single owner of Wake, capture, AEC/reference, playback, and re-arm lifecycle. Do not add Learning UI or the M1.5 seven-command router.

## Fixed service and candidate binding

Use the deployed XiaoZhi v0.9.6 service at `ws://192.168.3.100:7444/xiaozhi/v1/`. OTA discovery is plain HTTP at `http://192.168.3.100:7443/xiaozhi/ota/`. The deployment evidence binds server commit `f5ed1aaec88471ba00ac778045331514066d63dc`, image digest `sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`, SenseVoiceSmall, Ollama `qwen3.5:2b`, and EdgeTTS; re-record actual config/provider identities for the run. Do not treat network/OTA connectivity as a voice round.

Before preparing any M1 Candidate, read the live device partition table and record its hash/layout and device identity. Candidate build/flash assumptions must fit the actual layout; do not alter the partition table to fit firmware. Bind reviewed source SHA, app/ELF SHA, same device/session, server commit/image, config SHA and providers. Stop if identity cannot be established. Preserve the current single Voice Core owner.

## Opt-in M1 input diagnostics

S-08 Candidate `claw4-learning-v6-m1-wake-diag-s08-20260926-01` passed R-05 review and has a separate app-only device diagnostic record in [the S-08 device report](V6_M1_WAKE_DIAGNOSTIC_DEVICE_REPORT.md); that passive check is not a Voice round or M1 acceptance. It does not inherit any boot, endpoint, Wake, or Voice result from an older Candidate. Use the M1 input counters only on a separately reviewed M1 Candidate whose manifest and `sdkconfig` explicitly bind `m1_input_diagnostics=true` / `CONFIG_CLAW4_M1_INPUT_DIAGNOSTICS=y`. This option is M1-only and depends on `!CLAW4_M0_DIAGNOSTICS`; it does not enable the M0 local diagnostic application. Do not change WakeNet, AEC, recording, or audio routing to collect these counters.

The exact `Claw4Audio: M1_INPUT_DIAG` record is emitted about once per second while input reads are running:

```text
M1_INPUT_DIAG mic_n=<uint32> rms=<uint32> peak=<uint32> i2s_read_failures=<uint32>
```

`mic_n` counts microphone-slot PCM16 samples in the reporting interval; `rms` is their aggregate RMS and `peak` their maximum absolute PCM16 level. `i2s_read_failures` counts failed or short I2S reads in that interval. A failure may cause a report attempt before the normal interval, but the one-second throttle still applies. Fields are reset after an emitted record; absent records mean unavailable, not zero. These are bounded aggregate counters only; no PCM is retained or emitted.

`m1_voice_log_parser.py` accepts only the exact ESP-IDF-prefixed record shape, at most 256 characters per line, and values no greater than uint32. Its `m1_input_diag_window_count` counts accepted records, `m1_mic_samples_total` and `m1_i2s_read_failures_total` sum their interval counters with uint64 aggregate bounds, and `m1_rms_max` / `m1_peak_max` report the largest interval values. Malformed, oversized, or out-of-range records are ignored. `m1_wake_log_count` separately counts the exact `Application: Wake word detected` log marker. It is an observed application marker, not a completed voice turn. The legacy `wake_events_observed` is derived from distinct `V6M0: WAKE_DETECTED ...count=` values and is not an M1 Wake marker; do not substitute one field for the other.

Input counters and an ambient mic baseline can help distinguish silent/low-level input from observed read failures during an idle capture. They do not prove that WakeNet recognized speech, that AFE fetched usable input, that AEC works, or that any Voice stage passed. A zero read-failure aggregate means only that no accepted diagnostic record counted failures; no counters or no input reads remain `NOT_VERIFIED`. Keep this diagnostic observation separate from Voice round results.

## Post-flash autonomous checks (no speech stimulus)

Only after the candidate has passed its required independent review and a separate device-operation authorization is in force, perform the approved write and its readback. Bind the exact readback hash and device identity to the Candidate before interpreting its boot. Then check boot identity/ready markers and the running application/partition against that same Candidate, confirm that OTA discovery uses the fixed NAS endpoint and that no external endpoint is contacted, and collect an ambient mic baseline with no prompted speech. Do not use an older Candidate's identity, boot, endpoint, or diagnostic evidence. An endpoint counts as verified only when the corresponding connection is directly observed; because WebSocket creation may be Wake-triggered, an unobserved lazy WebSocket remains `NOT_VERIFIED` at this stage. These checks are not Voice Preflight rounds and do not establish Wake, AEC, or Voice PASS.

## M1-PREFLIGHT (2–3 rounds)

After the autonomous checks, begin a separate, user-assisted 2–3 round Voice Preflight with its candidate-specific input template and controlled synthetic speech only; use safe stimulus IDs and never put speech, transcript, credentials, or child data in evidence/logs. For each round observe Wake → Speech → ASR → LLM → TTS → Device Playback → Next Wake. Mark each result from direct evidence; missing/ambiguous observations remain `NOT_VERIFIED`. Record available heap/PSRAM minima; unavailable is null, never zero. The ambient baseline and diagnostic Wake marker do not count as rounds.

Observe whether TTS is captured again, whether the device self-dialogues, whether the AEC/reference signal demonstrably affects capture, whether near-end speech remains usable during playback, whether Wake resumes after playback, and whether the device crashes, WDTs, reboots, stalls, or loses heap/PSRAM unexpectedly. Logs alone showing reference samples do not prove AEC effectiveness. Preflight can identify instability, but is never M1 PASS and does not count toward the formal twenty rounds.

If preflight is stable, freeze the candidate, image, service config, and provider versions before starting formal evidence. Investigate failures before formal collection.

## Formal continuous 20 rounds and fault matrix

Use a fresh session and fill exactly 20 consecutive rounds numbered 1–20. Keep the same candidate, device boot/session, server image and commit, config digest, and provider versions. No reboot, service restart, manual recovery, dropped/renumbered turn, or identity change is allowed within the window. Use controlled synthetic short, long, silence, and interruption stimuli. Record ASR-final, LLM-first-token, TTS-first-audio and audible-first-response only when measured, retaining device/service monotonic clock domains separately. Record per-round minimum heap/PSRAM, flags, and fault matrix findings. Do not infer pass thresholds: none are defined in the existing evidence contract.

Any firmware modification during a formal twenty-round run invalidates the entire run. Freeze/discard its evidence, rebuild/rebind, and restart at round 1. A failed or interrupted run is not resumed at a later number. Keep AP outage/recovery `SKIPPED_BY_USER / NOT_VERIFIED`; do not dispatch that stimulus again.

Run `py -3.14 -B tools/v6/m1_round_evidence.py <input.json> <report.json>` on the completed formal template. Its `PASS` means evidence structure and explicit fault checks satisfy its declared rules; it does not establish voice quality or overall M1 acceptance. Preflight uses its separate template and is not accepted by this validator. The bounded `tools/v6/m1_voice_log_parser.py <log> <summary.json>` extracts only the existing Candidate ELF anchor, BOOT_READY, HEALTH, M0 WAKE_DETECTED, M1 input aggregates, exact M1 application Wake marker, and crash markers. Unknown lines never become evidence; its voice-flow fields remain `NOT_VERIFIED`. Keep raw device/server logs in approved private storage and publish only redacted aggregates/hashes.
