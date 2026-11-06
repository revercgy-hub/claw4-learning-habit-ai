# L-32 NAS stage profile report

Status: **three synthetic component trials observed; no live-device turn metric established**. This report adds a read-only interpretation of the isolated provider-stage probe. It does not revise the M1 evidence contract or Gate.

## Scope and identity

Root ran three fresh subprocess trials against the actual deployed Ollama provider and EdgeTTS stream/decode wrappers with a fixed synthetic input. The probe used the actual 177-character rendered prompt and validated source hashes. It serialized LLM completion before TTS; it is not the live WebSocket request pipeline, overlapping generation/playback, or device audio. No prompt text, generated answer text, audio payload, or credentials are reproduced here; only safe fixed-case counters/timings and hashes are retained.

At probe start (2026-11-06 12:42:53 UTC), the live NAS image was `sha256:9cd9b4a1f64e46adf5b320bcf58bbb6e39ade16ca46ed713c824058544945476`, container `5af9aa42affe8352084704820ffd06928b8b0b793fb50d3b58610ef5a7e97509`, config SHA256 `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911`, restart count 0. Post-probe identity remained unchanged. The model was already resident before the probe; follow-up at 12:47:46 UTC saw its same expiry (`2026-11-06 20:48:08 +08:00`). No forced unload or keep-alive change occurred, and this is not a controlled cold-versus-warm experiment. Root reports a later native load telemetry observation after natural model expiry separately; it is not included in this report pending its evidence record.

Probe input binds probe source `8b1cfc74f03f15c7447a91303184c099b1732788`, integrated source `8892ae63cbfdf096088f3f7b9b59a304bc71c565`, and script SHA256 `0dc4a12efa68fc45083e7c4bbe6946af8ee61c5e5db18d0f9ddf67d6b4cec43c`. R-15 independently returned `REVIEW=PASS` on immutable integration `8892ae63cbfdf096088f3f7b9b59a304bc71c565`. Full restored V6 integration Host suite: **190/190, zero failures/errors/skips**. The first root integration attempt's seven fixture failures arose from an old working fixture with CRLF line endings despite identical displayed text; fail-closed hash mismatch was correctly rejected. Safe repair evidence showed only CRLF drift; the canonical Git blob was restored without relaxing the SHA check or committing a fixture change. Root reran the full suite on restored input and obtained the cited 190/190 result. S-21's branch-level checks were 186/186 plus 11 targeted; L-29 targeted 19 and L-31 reported 175/175. Earlier L-28 child attempts are superseded: the first actual root retry was 8 tests, 7 pass and 1 fail, followed by Sol correction `8b1cfc7`; this sentence records execution history, not blame.

## Measurements

All times below are milliseconds relative to their named component's start, not cross-service or device-clock measurements. The three fixed synthetic results were `OK`, expected relation matched, TTS request count 1, and Opus decode error `null`. All trials emitted 33 Opus frames / 4,090 bytes / 31,680 decoded samples with the same Opus SHA256 `977db42621589a96e859e58f904a1f70007c0afebe00dfed989aa66f0b4039dd`.

| Trial | LLM first yield | LLM complete | TTS first MP3 chunk | Full TTS request | First Opus frame |
|---:|---:|---:|---:|---:|---:|
| 1 | 6798.94 | 6969.56 | 1266.23 | 1652.47 | 1856.11 |
| 2 | 209.69 | 380.74 | 1201.92 | 2586.35 | 2683.83 |
| 3 | 185.12 | 354.53 | 1622.91 | 2157.92 | 2257.40 |

The first trial's LLM component took materially longer than trials 2 and 3; the component probe alone cannot assign cause (for example, cold model load) or explain the earlier device's roughly eight-second text-marker interval. Trials are small, fixed-input observations, not statistical characterization or before/after optimization evidence. The TTS path is serialized after full answer generation, so its first-chunk timing cannot establish streaming overlap or actual device first sound.

After natural model expiry, a separate synthetic native API request (`natural-expiry-native.json`, 2026-11-06 12:49:09 UTC) confirmed the selected model was not resident before the request; no forced unload or keep-alive change was used. With the same 177-character prompt, it completed in 5577 ms wall / 5576.392658 ms reported total, including 4962.078590 ms load, 425.016 ms prompt evaluation, and 186.803 ms generation; 119 prompt / 6 output tokens, same seven-character output hash as the component trials. This demonstrates that model loading dominated this specific post-expiry native request and is consistent with the slower first component trial versus later trials. It does not identify the cause of the prior S-17 8.325-second marker interval, measure device voice latency, or constitute an OpenAI-stream A/B comparison. Follow-on S-22 work is `HOST_IN_PROGRESS`: assess internal connection-scoped background warmup with unchanged model/URL and no keep-alive/Voice API changes; no implementation, build, deployment, or result is claimed here.

## Device and M1 boundary

This component profile does **not** fill actual-turn ASR final, LLM first token on the live device request, TTS first audio on the live request, audible device first response, per-turn heap/PSRAM, AEC/reference effectiveness, near-end-during-playback, or UART-bound next-wake metrics. It does not change L-27's positive controlled audible answer and user-reported boundary Wake; the latter remains UART-unbound. Full preflight credit stays `0/2–3`, formal 20 rounds remain unstarted, M0 remains `CHANGES_REQUIRED`, and AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`. No test, fault injection, deployment, device operation, or Gate change is authorized by this report.

## Safe evidence references

Root evidence directory: `E:/v6/stage-profile-20261106-01/`.

| Evidence | SHA256 |
|---|---|
| `probe-inputs.json` | `a262621807c3c63705eac8b1d80c2134f96e5ce28e628e3e6c241a8d3b7d9d27` |
| `probe-result.json` | `0601b42f46cbeae0a3f0cf7360c9402bda6cb544d95778add15ec16699e9aefc` |
| `post-probe-identity.json` | `d305de30e92417f0bf2eaa03b90e60fd03f40e28f05d96aeac889e397036a4b6` |
| `residency-followup.json` | `bf5ec93af2243bb52716a5dda0f769af03df7081264d99183bd331c705db22d0` |
| `natural-expiry-native.json` | `03be8c929365ee59d0fe235ae4ac3c7a49d8236f7aa858b1870f1224e4263461` |
| Restored full suite log `E:/v6/v6-integration-20261106-02/unittest-full-restored.log` | `3337b6e7bc0c355517df4b373c7952c7aa85a1c0a8ed85d71a35f4ccbb27a53e9` |
| L-28 repair / L-29 parser replay evidence | Root-local safe evidence; no new private payload copied into repository |

See also [L-27 Preflight 04 report](V6_M1_VOICE_PREFLIGHT04_REPORT.md), [L-30 metrics readiness inventory](V6_M1_METRICS_READINESS.md), and [M1 plan](V6_M1_XIAOZHI_NAS_VOICE.md).
