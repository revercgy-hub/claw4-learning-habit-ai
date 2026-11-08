# L-38 NAS voice-flow evidence

> This report records isolated synthetic service-path observations. It does not represent a production change, a device turn, or a Voice Gate pass.

## Task and scope

- Task: `L-38-M1-NAS-VOICE-FLOW-EVIDENCE` (documentation-only evidence capture).
- Branch: `codex/l38-voice-flow-evidence`.
- Base: `7e11a70db3841e2c6b40f60f15bc6478b53bc3be`.
- Allowed files: this report, `docs/v6/V6_TASK_BOARD.md`, and `docs/project_management/TASK_BOARD.md` only.
- Scope: document the S-26 static source audit and root's isolated synthetic WebSocket/provider observations, their evidence identity, and limits. No source/ADR/Gate changes, NAS/SSH/device/COM7/Flash/build access, or private raw inputs.

## Findings

S-26's static source audit found that text-only `listen.detect` bypasses ASR but continues through the actual same-session Dialogue and Ollama/TTS WebSocket path. The first punctuation boundary can split the first number from the remainder of a numeric answer. TTS FIRST is a control marker and is distinct from arrival of actual Opus audio.

Root exercised the actual provider path over synthetic WebSocket sessions on an isolated server using the L-33 image `sha256:97c36ec5638a97d47756e906e0c449926ebf4192e701c6bece0ce7912c99d1c1` and config `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911`, with process-local instrumentation. The isolated server was stopped after the experiment; the live service identity stayed unchanged. These runs are service-path experiments and do not establish behavior of a real device session.

The first fresh-blue case had no system message in the actual LLM dialogue and returned 200 characters, not the requested exact response; the client timed out at 45 seconds. The other six initial WS cases had the expected rendered 177-character system prompt (SHA-256 `20d8fa8005be8045a72db012f54e6692a89830c068232d943efba1d53c76b005`). This supports a bounded first-turn prompt-readiness/lifecycle-race hypothesis: source event binding occurs in `_initialize_private_config_async` before `_initialize_components` executor, while TTS publishes before quick prompt. It does not prove this explains S-25's near-end response or marker interval. With the configured prompt present, fresh-blue, after-count-blue, and theme-history output still failed exact blue matching; theme-history produced a theme explanation. Both isolated count outputs split the leading `1` from the remainder at sentence segmentation.

The slow count's first-to-second TTS marker gap was 15,889.831 ms and maximum WS audio-packet gap was 15,171.471 ms. Its second Edge segment took 15,785.152 ms to succeed after three failed attempts; the batch had seven `NO_AUDIO` warnings and zero HTTP 403s. The fast count's corresponding marker gap was 2,764.142 ms, packet gap 1,963.968 ms, and second Edge segment 2,576.398 ms with no retries. The fast second text hash exactly matched S-25's historical second segment (48 characters). This does not explain S-25's 11.989643-second historical marker gap: S-25 exact-text/hash reclassification found zero outer retries on both historic segments; historical delay allocation remains unresolved.

The isolated retry behavior does not establish the cause of the historical 11.989643-second marker interval: S-25's exact-text/hash reclassification on the same active-container logger found outer retries were zero for both historic segments. Do not attribute that historical interval to the isolated retry mechanism.

The first in-memory literal-instruction-suffix comparison used the actual provider without a live change. Blue cases were 0/3 exact for both baseline and candidate; the violet holdout passed under candidate, while explanation and arithmetic controls passed. A narrower directed V2 comparison then returned exact blue for fresh and after-theme cases, but not after-count (2/3); violet, explanation, and arithmetic controls passed. This was an isolated actual-provider contrast, not a production prompt change. Configured prompt SHA-256 was `502530514e0fc8140e83552132c1f7a46dcdf7f0b29588cafbb8304218fff5df`; tested literal suffix SHA-256 was `17f1b8dd389c1dd308cfaf67513becc030e3f1d23eadfc899d3bac2f0445e246`.

## Evidence boundary and current statuses

- No production code, prompt, configuration, or live service change is represented by these observations.
- A missing system message was observed in the first isolated turn. First-turn prompt readiness is a supported investigation hypothesis; no root cause or fix is claimed.
- No device interaction, Flash, AEC conclusion, or end-to-end device latency measurement is represented.
- S-24 DEVICE status as recorded in S-25 remains separate and unchanged.
- M0 remains `CHANGES_REQUIRED`; AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`.
- Voice Preflight remains `0/2–3`; formal 20-round run remains `NOT_STARTED`.
- No M1 acceptance or Voice Gate pass is implied.

## Acceptance checklist

- [x] Work is limited to the three documentation paths.
- [x] Static source observations are distinguished from synthetic live-provider observations.
- [x] Historical S-25 marker timing is not causally explained by isolated retry observations.
- [x] Final safe summary, artifact hashes/status, and directed comparison result recorded and cross-checked.
- [x] Relative links, evidence identity, status consistency, and `git diff --check` verified.
- [x] Commit contains only the allowlisted documentation paths.

## Validation and work record

- Base check: `git rev-parse HEAD` → `7e11a70db3841e2c6b40f60f15bc6478b53bc3be`.
- Safe summary: `E:/v6/s26-voice-flow-20261108-01/s26-safe-summary.json`, SHA-256 `58a20b705db69b34fb2780e0bad9cb9d25c97c3b752d97e0101f0d0182b04179`.
- Key evidence SHA-256: `provider-contrast-result.json` `1a9de3dfb03c2661db197937ded1c1e002aee8f0c32adf0e85858a1824ee8084`; `provider-v2-result.json` `02df1d4b08d69522f32f49fcf8dd9c5ae5b34b3960da79bb942604313ec6621d`; `bounded-safe-observations.json` `286bc09419a4493e401ada2f3c5826c6ca017650a59cf0400b4d7f5a4f4af843`; `historical-count-stage.json` `f303d25bbaa373c485dd0cffa6b6b2902977853ea2604e41c1b5da49034b97bf`.
- Documentation validation: report target and cross-board links exist; safe summary SHA-256 recomputed and matched; current statuses cross-checked against S-25; `git diff --check` passed. The 209/209 Host result remains historical S-25 evidence and was not rerun here.

## Remaining risks and review focus

The prompt-format behavior, segmentation, and TTS retry observations are limited to the isolated synthetic provider path and instrumentation used. They do not establish general answer quality, production behavior, device playback timing, or the cause of S-25's measured interval. The missing first-turn system prompt warrants a bounded readiness investigation; it is not a proven race or established explanation for S-25. Review that the safe summary supports each value and that all statuses remain unchanged.
