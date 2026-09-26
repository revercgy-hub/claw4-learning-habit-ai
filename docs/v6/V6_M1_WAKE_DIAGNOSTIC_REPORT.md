# S-06-WAKE-DIAG: read-only wake-path evidence audit

Status: **DIAG_COMPLETE / ROOT_CAUSE_UNRESOLVED** (2026-09-26). No source, build, candidate or device state was changed. Claw4 remains powered off and COM7 is released.

## Evidence examined

The only device evidence examined was the existing private serial capture `E:/v6/s05-ws/r03-continuous-probe.bin` from Candidate `claw4-learning-v6-m1-preflight-endpoint-s04-r03-20260925-03` (app SHA-256 `1735fa6ca019f13e7cc1056477ade93c08390b438fd04f7b73ae5be984cf07d5`, capture SHA-256 `bbb4b7f64c0207e12665685ee139307a9cd1df830b518664883789d7a685f25a`). The scan emitted aggregate counts only; it did not print speech, raw lines or audio.

- The 326.25-second capture contains 324 `TX_REFERENCE` records. Every record reports `active=0`, `overflow=0`, `underflow=0`, `stale_writes=0`, `discarded=0`, and `pending=0`. This is consistent with no playback-reference queue activity during an idle wake-only probe; it says nothing about microphone signal level.
- There are zero `INPUT ch=` records because the M1 candidate has `CONFIG_CLAW4_M0_DIAGNOSTICS` disabled. Consequently the capture does not report mic RMS/peak or I2S read failures.
- Wake, WebSocket, and NAS port 7444 marker counts remain zero, as recorded in the S-05-WS-VERIFY report. There are no AFE-fetch failure markers in this log, but absence of an uninstrumented marker is not evidence that AFE fetch ran successfully.

## Source-path review

The reviewed staged source enables WakeNet in Idle, reads stereo input and feeds AFE on the audio task, and emits `Wake word detected` before attempting the lazy WebSocket path. No deterministic source defect was found. The selected model is `wn9_nihaoxiaozhi_tts`; device AEC and WakeNet remain enabled. The reference slot is software playback audio and is expected to be zero while idle. These facts do not establish that the physical mic delivered adequate PCM or that AFE fetched it.

## Conclusion and next step

This one synchronized user-reported attempt was not observed as a Wake event, but the cause remains unresolved among signal level, input/read path, AFE processing and model recognition. It does not establish a general Wake failure, and no complete Voice Preflight round occurred (`0/2–3`); M1 remains NOT_VERIFIED and M0 remains CHANGES_REQUIRED.

A bounded follow-up should expose only aggregate input RMS/peak and I2S read-failure counters for an M1 diagnostic Candidate, without enabling the M0 local diagnostic application, recording PCM, or changing Wake/AEC behavior. It should then pass independent source/build review before a single-owner app-only write/readback and one synchronized near-field wake probe. No hardware action is part of S-06.