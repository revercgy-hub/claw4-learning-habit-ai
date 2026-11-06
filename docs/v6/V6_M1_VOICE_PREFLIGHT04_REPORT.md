# L-27 S-17 Voice Preflight 04 evidence

Status: **evidence recorded; successful controlled audible exchange observed, full Preflight gate incomplete**. The 2026-11-06 300-second continuous capture and same-window NAS safe classification are recorded here. Credited full-gate rounds remain `0/2–3`, the formal 20-round run has not started, and AEC, near-end, and a UART-bound next Wake remain unverified.

## Candidate and capture identity

This was an observation of the existing S-17 bundle `claw4-learning-v6-m1-model-repair-s17-20261018-01`; no firmware build or Flash was performed. The capture used an explicit USB reset, which is not a cold boot. One valid startup anchor matched the expected ELF prefix `33bcb2d01`; the expected `wn9_nihaoxiaozhi_tts` model, NAS OTA, and WS endpoint were observed. The initial buffered uptime before the anchor remains private and is not treated as a new session. A single 2 ms timestamp regression occurred after the anchor; there was no additional boot/reset anchor.

The capture ran for 300,000 ms and ended normally at its configured maximum: 177,601 bytes across 2,897 chunks. Private raw capture SHA256: `5bc67d123b5aa64867ea0e13422a8ed696120d5b5a233629bac8ce9969d8df91`. Raw UART, speech, transcripts, endpoint strings, and credentials are not included in this report. Safe JSON evidence is in `E:/v6/s17-asr-voice-20261106-01/`; `preflight-04-safe-evidence.json` is the bounded semantic summary. The initial `run-preparation.json` is an interim snapshot and is not used as final capture status.

## Observed interaction

The safe timeline records an initial Wake followed by greeting/listening activity. At uptime 100,027 ms, an ASR marker matched the controlled question after normalization. At 108,352 ms, a TTS text marker matched the expected answer. This is an 8,325 ms interval from ASR text marker to TTS text marker, not measured first-audio playback latency. The operator confirmed an immediate Wake and hearing the correct answer, with an approximate wait of eight seconds. This is a positive observed audible-response result for the controlled exchange.

The state returned from speaking to listening at 110,495 ms. After a long interval, it changed from listening to speaking at 230,121 ms. Same-window NAS classification found an `LLM_INPUT` event at 11:45:42.907 UTC, 36 characters with hash `25883edccc1a6c9214f91fdd64b384be5c8e7858898995f7f62007df74715bd5`; its hash exactly matches the effective configured/fallback end prompt reconstructed from the reviewed receive-audio source path. The configured silence timeout was 120 seconds. The reviewed `receiveAudioHandle.py` source (SHA256 `4493dfd77c1bfa7be1a79bb293dbdbf8cad64a6eb2bc53efca3e30efefcb47f1`) applies that prompt on `no_voice_close_connect` after the timeout. This supports scheduled close/farewell as the explanation for the late transition; it was not a second ASR result. The timeline later observed listening at 249,076 ms and idle at 249,150 ms. At the capture boundary, the operator reported a second immediate Wake, but no second Wake event is bound in the final UART capture. Record this as `USER_REPORTED_NEXT_WAKE / UART_NOT_VERIFIED_CAPTURE_BOUNDARY`: retain the user's positive report without treating it as another verified Wake or a failure; no repeat attempt is requested.

## Input diagnostics and verification limits

The parser recorded 286 input windows, 4,596,320 microphone samples, RMS max 2,615, I2S read failures 0, and crash markers 0. Heap and PSRAM minima were unavailable. One AFE detection was recorded in the window; this count alone does not establish AEC effectiveness. No extra ASR appeared in the captured window, but that does not establish AEC PASS or exclude near-end speech in other conditions.

The same-window NAS-safe records align one `ASR_RESULT` and one `LLM_INPUT`; both contain 7 characters and share hash `d5e50370882978688a071516f16ebee834792105dc9e72cb7235c4a4e78bcf4f`. The event-classification pass normalizes the controlled question and matches both markers. A naive safe-window numeric classifier had `controlled_question_match=false`; the classification correction explains that discrepancy without exposing text. NAS log marker times were ASR at 11:43:32.809799696 UTC and LLM input at 11:43:32.810205549 UTC, a 0.406 ms marker interval, not end-to-end ASR processing latency. NAS `TTS_FIRST_AUDIO` marker was 11:43:41.136304313 UTC, about 8.326 seconds after the ASR marker. This is a server send marker, not proof of speaker playback timing; the operator's report supports that audio was heard.

The user-reported correct, audible answer and matching expected-answer text marker are positive evidence for this exchange. The ASR marker's normalized match establishes the recognized controlled question text in the log. The text marker is not an audio-playback timestamp. The capture-boundary next Wake is not UART-bound. The full round gate still requires its remaining evidence, including AEC, near-end, and complete next-Wake binding, so the conservative gate counter remains **0/2–3**, despite the successful audible exchange. Formal 20 rounds remain unstarted.

## NAS correlation and phase state

The same-window NAS safe file showed the expected image `sha256:9cd9b4a1f64e46adf5b320bcf58bbb6e39ade16ca46ed713c824058544945476`, container `5af9aa42affe8352084704820ffd06928b8b0b793fb50d3b58610ef5a7e97509`, and restart count 0. HTTP 403, traceback, and interrupt log counts were all 0. Separate pre/post identity records confirm that config SHA256 remained `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911`. L-25's synthetic service verification remains valid but separate from device voice evidence. The late transition is consistent with the code-configured silence timeout and scheduled end prompt, not a second ASR/user question.

A separate read-only native chat telemetry probe (`native-telemetry-and-postidentity.json`) used a synthetic native API request, not the actual production server request or device path. With streaming disabled, rendered prompt length 177, `think=false`, token cap 128, and temperature 0.3, it completed in 551 ms wall time (549.86 ms reported total; load 1.12 ms, prompt evaluation 290.055 ms, generation 176.318 ms; 119 prompt and 6 output tokens). The model was already resident before the probe; keep-alive was not changed and the model was not forcibly unloaded. This probe does not show that cold start, the LLM, or TTS caused the actual 8.325-second marker interval. A future diagnosis would need actual provider-stream and TTS stage timings before selecting an optimization. Post-observation identity remained the same image/config/container, with restart count 0; no model, provider, or configuration change was made.

M0 remains `CHANGES_REQUIRED`; AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`. M1 is not accepted. No device retest is being scheduled from this report; COM7 has been released and no further user repetition is requested here.

## Evidence references

- [L-25 NAS ASR/prompt repair evidence](V6_M1_ASR_PROMPT_REPAIR_REPORT.md) — verified NAS service scope and limitations.
- [Safe Preflight 04 semantic evidence](<E:/v6/s17-asr-voice-20261106-01/preflight-04-safe-evidence.json>) — SHA256 `ce7e4720f6954c21cb954e2c367a4dd8b368e9f9558dc369fe6e8677851b3bc2`.
- [Same-window NAS event classification](<E:/v6/s17-asr-voice-20261106-01/preflight-04-nas-event-classification.json>) — SHA256 `bdf7f03a5521ac27819ccd38196990c08adbed8ca2e01e245c212ccd518fe921`.
- [Same-window NAS safe summary](<E:/v6/s17-asr-voice-20261106-01/preflight-04-nas-safe-window.json>) — SHA256 `b9a532098413f82137a56a83b5a96c9fbb08026ebf0233e35aea9356bc2d1afc`.
- [Capture state](<E:/v6/s17-asr-voice-20261106-01/preflight-04-state.json>) — SHA256 `53e870ab927a86e2e31e99f0ea2d51ef50693517506e0be6fe19d386224543ca`.
- [Synthetic native telemetry and post-observation identity](<E:/v6/s17-asr-voice-20261106-01/native-telemetry-and-postidentity.json>) — SHA256 `102d4defaca96d3c8380c51f79532b611eb76ca8e85334e9dadacffa3a051587`.

These SHA256 values were recomputed from the listed safe JSON files; the NAS runtime manifest and all hashes in its evidence map also matched on disk. The raw UART file was not read for this documentation task; its SHA256 above is copied only from the safe evidence record.
