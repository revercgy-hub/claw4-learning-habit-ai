# L-33 Ollama prewarm evidence report

## Scope and decision

Task: L-33, documentation-only evidence sync. Candidate `claw4-nas-prewarm-20261106-01`. Source is the immutable reviewed integration `d587f415e29fc3385eebd1858efce8847a5cba60`, with S-22 fix `f95ce4679e8842cf21b68353ed81a99e2e8b0dd0`. The A-07 contract remains the governing boundary: internal connection-scoped background preload of the existing model; no public Voice/API/endpoint/prompt/model-selection changes, keep-alive change, device work, or M1.5/UI work.

R-16 returned `PASS` for the immutable integration. Targeted tests passed `9/9`; integrated V6 Host suite passed `199/199` with zero failures/errors/skips. The new image was built 2026-11-06 and exact identity recorded. Isolated synthetic WebSocket probe ran 2026-11-08: `2/2` synthetic text-to-server-audio cases passed. For the first case the selected model was absent before connect and at `hello` (hello in 7 ms), then read-only residency observation found it resident before sending the synthetic question (8472 ms from connect). First Opus was observed 2173 ms and 1513 ms after the two questions. Both cases matched expected answer, reached TTS stop, decoded respectively 31,680 and 18,240 samples with zero decode errors.

This is isolated synthetic service-path evidence, not a controlled end-to-end cold/warm A/B, device turn, ASR validation, or speaker playback measurement. No helper INFO marker was visible. The report relies on read-only model residency observation and binding to the reviewed source; it makes no claim about helper load duration or a confirmed resident-branch log.

## Root-owned rollout (2026-11-08 Asia/Shanghai)


The guarded rollout completed with `NAS_SERVICE_VERIFIED`. The owner-only guard's first attempt stopped before any service mutation: its identity script used `hashlib.file_digest`, unavailable in the candidate container Python 3.10 runtime. The owner corrected the evidence script to stream the file hash and retried with the same reviewed image and tests. No live service mutation occurred on the failed attempt.

The active container is `6f7389a8a62b3215b48404753fc66278712b1e4c7a2f8778109f47f08b38cc20`, running image `sha256:97c36ec5638a97d47756e906e0c449926ebf4192e701c6bece0ce7912c99d1c1`, restart count 0. Config SHA remains `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911`. Compose delta is limited to image/build; the verified host mapping remains singular. Local HTTP returned 200 and the expected WS endpoint. Rollback target is the prior image `sha256:9cd9b4a1f64e46adf5b320bcf58bbb6e39ade16ca46ed713c824058544945476`, tag `claw4-xiaozhi-v6:voice-asr-20261020-01`, with the prior container ID `5af9aa42affe8352084704820ffd06928b8b0b793fb50d3b58610ef5a7e97509` retained as historical identity. Rollback recreates the `xiaozhi` service from the prior image; it does not restart that old container. The root-owned exact rollback in `/vol2/1008/docker/xiaozhi-v6` is `docker compose -f compose.yaml -f compose.asr-voice-fix.yaml up -d --no-build --pull never --force-recreate xiaozhi`. Before running it, verify config SHA `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911` and that the rollback tag resolves to the image ID above; after running, verify the config SHA remains unchanged and confirm full image identity, running state and OTA. This recreates only `xiaozhi`, without build or pull. Rollback was not executed.

Active service synthetic WS probe passed `2/2`: expected answer and TTS stop observed, 0 HTTP 403, 0 Opus decode errors; question-to-first-Opus was 1932 ms and 1835 ms, with 31,680 and 32,640 decoded samples. Model was resident before connect and at hello in both active cases. These are synthetic service timings, not device/ASR/end-to-end latency. The helper INFO marker was not visible, so do not infer helper load duration or log a confirmed resident-branch execution.

## Evidence provenance

Safe exact-byte artifacts under `E:/v6/prewarm-20261106-01/`:

| Artifact | SHA-256 |
| --- | --- |
| `build-identity.json` | `3225dcde2e7451a70fbd105798999d47f2c886d6183f85869dda772b20e39a47` |
| `staging-manifest.json` | `9f65cb085c3bd2e548b90a7a2ae039e9b4e782f3e0b19084976442666bce2450` |
| `isolated-ws-result.json` | `9ee0d7ae1715ca05e865087c3df08ae18566cbf8a0dadffbc29749669e2defd5` |
| `isolated-runtime-identity.json` | `c9a99c676dafecae369bf16d1a6c4d955896855ef264b0fcd334186101439b58` |
| `predeployment-manifest.json` | `512c0f24aad1a9323ca76bba0ba9e7879217c6eb8ff4c68b729c1257191dfaaa` |
| `postdeployment-identity.json` | `c49f46afc6692caa8a29d09e1729bf398fda9d3eef004d7c903786d344144e14` |
| `active-ws-result.json` | `2dcac649a695018ac6160ea59b6d4c7347cccb4fb015ca43224f257d3b904e11` |
| `rollout-result.json` | `009acaf0d267095c34f4f079b3ab41d81e1edd53f7d177f5806ac6787add5c93` |
| `rollout-result.json.attempt1` | `8e0bef43940233c0fbf6b19811a667085ef1f366459628c44c479e4b80c52f95` (failed guard before service mutation) |

Review evidence: `E:/v6/r16-prewarm-review-20261106-01/finalreview.md`, R-16 `PASS`, reviewed immutable integration `d587f415e29fc3385eebd1858efce8847a5cba60` and directed fix `f95ce4679e8842cf21b68353ed81a99e2e8b0dd0`.

## Gate status and limits

- `NAS_SERVICE_VERIFIED`: PASS for the scoped synthetic service path and current container identity.
- `M0`: `CHANGES_REQUIRED`.
- AP outage/recovery: `SKIPPED_BY_USER / NOT_VERIFIED`.
- Voice Preflight: `0/2–3`; formal 20-round run: `NOT_STARTED`.
- Device and ASR: `NOT_VERIFIED` by this work; no device test or Flash was performed.
- M1: not accepted. No M1.5 Router or Learning UI work was performed or authorized here.

L-25 remains valid historical evidence for its prior runtime; it does not describe the current service. L-27 remains positive prior device evidence bound to its own runtime/session; it does not transfer to this service candidate. No prior service or device result is inherited.

## Work record

Task: L-33. Branch: `codex/l33-prewarm-evidence`. Base: `d587f415e29fc3385eebd1858efce8847a5cba60`. Files changed: this report, `V6_TASK_BOARD.md`, and `docs/project_management/TASK_BOARD.md` only. Documentation checks: relative links, hashes/evidence provenance and status consistency, then `git diff --check`. No hardware, NAS, SSH, build, or runtime operations were performed by this documentation task.
