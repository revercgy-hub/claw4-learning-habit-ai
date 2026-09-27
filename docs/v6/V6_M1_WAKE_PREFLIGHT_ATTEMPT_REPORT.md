# S-08 user-assisted M1 Wake preflight attempt

**Status:** `PREFLIGHT_BLOCKED_WAKE_NOT_OBSERVED / ROOT_CAUSE_UNRESOLVED` (2026-09-27)

**Candidate:** `claw4-learning-v6-m1-wake-diag-s08-20260926-01`

**Candidate app SHA-256:** `5619f2f2cd19cc42746fe1025a7c6390e037313573e1203093dfbf057889b27c`

**Firmware source:** `85b2265b65a09b67d9eabe9da5373bf18468161b`
**Capture source root HEAD:** `c2f5c5f`

This report records one user-assisted attempt using the same reviewed S-08 Candidate. It supplements the earlier passive S-08 device check with a continuous capture and two operator-reported attempts. It does not identify a root cause, establish a general WakeNet failure, or inherit evidence from another Candidate.

## Candidate identity and capture

Before capture, read-only `esptool verify-flash` verified 3,136,144 bytes at `0x200000` against the reviewed app SHA above. The device was identified as ESP32-P4 revision 1.3, MAC `80:f1:b2:d2:ed:14`. This was verification only; no flash write was performed for this attempt.

One continuous UART capture ran for 201.328 seconds after an explicit USB reset. The capture completed with 63,958 raw bytes and 1,141 contiguous index chunks. The ELF prefix `fc07bcb12` was observed once, and the device booted from `ota_0`. NAS OTA HTTP endpoint `192.168.3.100:7443` was observed once. No external endpoint, NAS WebSocket port 7444, Wake event, or crash marker was observed in this capture.

## Input diagnostics and reported attempts

The parser found 188 `M1_INPUT_DIAG` windows covering 3,023,200 microphone samples. Maximum interval RMS was 2,507 and maximum interval peak was 21,979; I2S read failures were zero. Heap and PSRAM values were unavailable. These are input aggregates only; they do not establish the quality of the speech signal or any downstream Voice stage.

The user reported two attempts, summarized without reproducing spoken content:

1. The user reported attempting Wake without a device response. The attempt was intended as Wake followed by a question, but only the Wake attempt was reported; the first-round question utterance is `NOT_VERIFIED`.
2. At roughly 20–30 cm, the user confirmed no light or screen response. Audio feedback was not separately confirmed.

The onset time of either utterance is unknown, so neither report can be bound to a precise acoustic frame or parser window. For the second report, the preceding 30-second aggregate had maximum RMS 381 and peak 4,870. This interval is context only and does not prove that the utterance was captured or recognized.

## Preflight outcome and boundaries

No complete Wake → Speech → ASR → LLM → TTS → Device Playback → Next Wake round completed. Preflight remains `0/2–3`; formal continuous 20-round testing never started. The state is `PREFLIGHT_BLOCKED_WAKE_NOT_OBSERVED / ROOT_CAUSE_UNRESOLVED`. It must not be generalized to `WakeNet FAIL`, and it is not `M1 PASS`.

The evidence does not distinguish among acoustic timing/level, input capture, AFE processing, or model recognition. No such cause is claimed. NAS OTA HTTP was observed; NAS WebSocket 7444 and every Voice stage after Wake remain `NOT_VERIFIED`. M0 remains `CHANGES_REQUIRED`; AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`. No result from an older Candidate is inherited.

## Private evidence binding

The following artifacts remain in private storage. Their raw contents, speech, transcripts, credentials, and device log lines are not included in this repository:

| Private artifact | SHA-256 |
| --- | --- |
| `E:/v6/s08-device/s08-preflight-identity-verify.txt` | `c67dbd6dbfd684ad4631125e36c573f7f40d33292a726ac11948484b4462ac70` |
| `E:/v6/s08-preflight-20260927-01/uart.raw` | `6a26405468d8c6640fae9e44d936b5b71bca302116993d35f91a9ff68eab9459` |
| `E:/v6/s08-preflight-20260927-01/uart-index.jsonl` | `9aa4e476772ad7080aea169fbc1b593b315d2f01d5c4f1f72770d89f13bc402c` |
| `E:/v6/s08-preflight-20260927-01/uart-state.json` | `5cdacade4db1f7d9169393618b09dc4d0bfd50d74c6099ae1f6beedd6ec5d76c` |
| `E:/v6/s08-preflight-20260927-01/uart-summary.json` | `a65fd73bba3a792c3295dded0819527820b48dcd1ecbf82c8859e310bbd8e39a` |
| `E:/v6/s08-preflight-20260927-01/operator-events.jsonl` | `c672f0dde361fc307bb669e6e8cd0092540ff1630d032613126bf8d989719d67` |

The separate passive diagnostic report remains available at [S-08 device check](V6_M1_WAKE_DIAGNOSTIC_DEVICE_REPORT.md); its earlier ambient-only capture is not merged into this attempt's counters or session. The operating procedure is [M1 Voice runbook](V6_M1_VOICE_RUNBOOK.md).
