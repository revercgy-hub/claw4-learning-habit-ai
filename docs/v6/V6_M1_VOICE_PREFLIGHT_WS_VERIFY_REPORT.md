# S-05-WS-VERIFY: continuous wake-only endpoint observation

Status: **BLOCKED_WS_ENDPOINT_NOT_VERIFIED** (2026-09-25). The R-03 candidate was not rebuilt or reflashed. A single uninterrupted passive COM7 capture covered the user's wake-word attempt and the subsequent observation period. The serial log contains no Wake or WebSocket marker, so the NAS WebSocket endpoint remains unverified. No complete Voice Preflight round was started; this is **0/2–3 rounds**, not an M1 result.

## Capture and stimulus binding

- Candidate remains `claw4-learning-v6-m1-preflight-endpoint-s04-r03-20260925-03`; app SHA-256 `1735fa6ca019f13e7cc1056477ade93c08390b438fd04f7b73ae5be984cf07d5`, ELF SHA-256 `33955b7af0e8890fdae9a5fde33be82496722c85216301f58e80b7ae333e8391`. The previous R-03 app-only write/readback and NAS OTA HTTP `192.168.3.100:7443` observation remain valid evidence for this unchanged image.
- One passive serial process opened COM7 without resetting the device at `2026-09-25T13:11:04.309Z`. It ran continuously for 326.25 seconds and ended at `13:16:30.564Z`. The user prompt was issued at approximately `13:11:59Z`; the user reported the single “你好小智” attempt by replying at `13:15:10Z`. The capture therefore spans the entire possible attempt interval and continued for 80 seconds after the reply.
- The append index contains 712 records; the largest adjacent index timestamp gap is 1.006 seconds. This was one file and one open capture process, with no capture-window gap. The raw private log is `E:/v6/s05-ws/r03-continuous-probe.bin`, SHA-256 `bbb4b7f64c0207e12665685ee139307a9cd1df830b518664883789d7a685f25a`. The index SHA-256 is `fd5e402e90596d3fcb6b6349a896bc4e4aff110e5ad1c514667b15b015e8f37f`; final state SHA should be recomputed after archiving this report.
- A bounded marker scan found zero Wake, WebSocket, NAS host/port, external Tenclass, MQTT, crash/WDT, or reboot markers. This means the attempted wake word was not observed as a Wake event in this capture. Its exact utterance time and acoustic level at the microphone are not measured, so this single result does not establish a general Wake subsystem failure. Capture-state SHA-256 is `90d24b12d2b77ce11d8c30f7fe3f43df5e538649d03ba760a2f090a5b8468610`; the user-reply marker SHA-256 is `9c3624515534e230659233dab78152625a6a1885ab276317879ea66211b88da2`.

## Gate result

The prior boot proved NAS OTA HTTP connectivity, but a wake-word event is the only available path on this Claw4 board to open the WebSocket. The current observation produced no Wake marker and no WebSocket connection, so `ws://192.168.3.100:7444/xiaozhi/v1/` remains `NOT_VERIFIED`. The capture showed no external connection, MQTT, crash or reboot. No speech flow, ASR, LLM, TTS or playback round occurred; AEC effectiveness, playback-time near-end speech, next Wake, heap/PSRAM trend and stability remain `NOT_VERIFIED`.

No Flash operation or source change occurred during this task. The previous `ota_0` identity/readback evidence is unchanged. The capture process closed COM7 at completion. The user was asked to power off the device after testing; confirmation was still pending when this report was written.

Follow-up (2026-09-26): the user confirmed Claw4 is powered off. COM7 remains released. Candidate20 AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`; M0 remains `CHANGES_REQUIRED`.
