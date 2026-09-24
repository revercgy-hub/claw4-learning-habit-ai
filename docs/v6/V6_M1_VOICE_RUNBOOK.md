# M1 Voice operator runbook

This runbook supports evidence collection; it does not change Voice architecture. Voice Core remains the single owner of Wake, capture, AEC/reference, playback, and re-arm lifecycle. Do not add Learning UI or the M1.5 seven-command router.

## Fixed service and candidate binding

Use the deployed XiaoZhi v0.9.6 service at `ws://192.168.3.100:7444/xiaozhi/v1/`. OTA discovery is plain HTTP at `http://192.168.3.100:7443/xiaozhi/ota/`. The deployment evidence binds server commit `f5ed1aaec88471ba00ac778045331514066d63dc`, image digest `sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`, SenseVoiceSmall, Ollama `qwen3.5:2b`, and EdgeTTS; re-record actual config/provider identities for the run. Do not treat network/OTA connectivity as a voice round.

Before preparing any M1 Candidate, read the live device partition table and record its hash/layout and device identity. Candidate build/flash assumptions must fit the actual layout; do not alter the partition table to fit firmware. Bind reviewed source SHA, app/ELF SHA, same device/session, server commit/image, config SHA and providers. Stop if identity cannot be established. Preserve the current single Voice Core owner.

## M1-PREFLIGHT (2–3 rounds)

Start with the candidate-specific 2–3 round input template. Use controlled synthetic speech only and safe stimulus IDs; never put speech, transcript, credentials, or child data in evidence/logs. For each round observe Wake → Speech → ASR → LLM → TTS → Device Playback → Next Wake. Mark each result from direct evidence; missing/ambiguous observations remain `NOT_VERIFIED`. Record available heap/PSRAM minima; unavailable is null, never zero.

Observe whether TTS is captured again, whether the device self-dialogues, whether the AEC/reference signal demonstrably affects capture, whether near-end speech remains usable during playback, whether Wake resumes after playback, and whether the device crashes, WDTs, reboots, stalls, or loses heap/PSRAM unexpectedly. Logs alone showing reference samples do not prove AEC effectiveness. Preflight can identify instability, but is never M1 PASS and does not count toward the formal twenty rounds.

If preflight is stable, freeze the candidate, image, service config, and provider versions before starting formal evidence. Investigate failures before formal collection.

## Formal continuous 20 rounds and fault matrix

Use a fresh session and fill exactly 20 consecutive rounds numbered 1–20. Keep the same candidate, device boot/session, server image and commit, config digest, and provider versions. No reboot, service restart, manual recovery, dropped/renumbered turn, or identity change is allowed within the window. Use controlled synthetic short, long, silence, and interruption stimuli. Record ASR-final, LLM-first-token, TTS-first-audio and audible-first-response only when measured, retaining device/service monotonic clock domains separately. Record per-round minimum heap/PSRAM, flags, and fault matrix findings. Do not infer pass thresholds: none are defined in the existing evidence contract.

Any firmware modification during a formal twenty-round run invalidates the entire run. Freeze/discard its evidence, rebuild/rebind, and restart at round 1. A failed or interrupted run is not resumed at a later number. Keep AP outage/recovery `SKIPPED_BY_USER / NOT_VERIFIED`; do not dispatch that stimulus again.

Run `py -3.14 -B tools/v6/m1_round_evidence.py <input.json> <report.json>` on the completed formal template. Its `PASS` means evidence structure and explicit fault checks satisfy its declared rules; it does not establish voice quality or overall M1 acceptance. Preflight uses its separate template and is not accepted by this validator. The bounded `m1_voice_log_parser.py <log> <summary.json>` extracts only existing Candidate ELF anchor, BOOT_READY, HEALTH, WAKE_DETECTED and crash markers. Unknown lines never become evidence; its voice-flow fields remain `NOT_VERIFIED`. Keep raw device/server logs in approved private storage and publish only redacted aggregates/hashes.
