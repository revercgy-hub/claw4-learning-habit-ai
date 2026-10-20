# L-18 NAS Edge TTS repair evidence report

记录日期：2026-10-20。此报告补充 S-17 首次 Wake/提问后未听到答案所发现的 NAS TTS 403 阻断，并记录后续 `preflight-02` 设备观察。结论限于 `NAS_TTS_SERVICE_REPAIR=PASS`；不宣称设备已播放答案、Preflight 完成或 M1 验收通过。

## 变更与服务身份

L-17（source commit `812b12905cda01843f5aeb2d80835abb9bc2b084`）基于原始 XiaoZhi v0.9.6 digest 固定镜像构建唯一的本地修复镜像 `claw4-xiaozhi-v6:tts-731-20261018-01`，image ID 为 `sha256:acf7314b488ad7e49232797fb42711faad637c00d29142ec782124a92b1d9291`。独立审查 R-11 为 `PASS`。候选保留原 base `ghcr.io/xinnan-tech/xiaozhi-esp32-server:server_0.9.6@sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`，只改变 `edge-tts`：从 7.2.6 升至 7.3.1；未升级其他依赖。

7.3.1 wheel SHA-256 为 `2a3c79d6235a7a736d681233823ce5492e829b43eb47beae49aa181237a8f9a4`。上游 [PR #492](https://github.com/rany2/edge-tts/pull/492) 增加 Sec-MS-GEC v2 token 支持，并记录了相关 403 handshake 修复；[7.3.1 release notes](https://github.com/rany2/edge-tts/releases/tag/7.3.1) 记录 locale-dependent server-date parsing 修复。此次本地结果由下述 NAS/provider 探针支持，不将上游变更描述为完整设备语音修复。

NAS rollout 后服务运行于该本地 image ID，restart count 为 0，OTA HTTP 状态为 200。候选 merged Compose 与原服务配置的差异限于 image/build：merged Compose SHA-256 为 `ffcdb55a0f23e7cf8927684919b751d008d9565a047971d988be122adbf07d0b`；NAS-local config SHA-256 前后均为 `3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438`；原 `compose.yaml` SHA-256 保持 `8e420b89d0381608b5833078168c53c3c41b741fc9f0e971f2630c6d3d488f91`。因此 TTS provider/voice 配置、ports、volumes 和其余 service 配置保持原身份。旧 image `sha256:d8ab77bb1f2b212ee3f1d45fb9d9f9faf953be46c13c70ff6537044dd0b1e27e` 仍保留作回退目标；没有执行 rollback。回退命令见 [NAS deployment README](../../deploy/v6/xiaozhi-nas/README.md)，仅使用原 Compose 文件重建 `xiaozhi` 服务。

## 合成探针与判定边界

唯一用于判定 `NAS_TTS_SERVICE_REPAIR=PASS` 的音频证据是 postdeployment identity 中运行中 NAS service/provider probe：它报告生成 11,664 bytes，实际音频 SHA-256 为 `6cd8ac8ecf1395b976c0dd170c1ccceb8428311253513f0b84c1eb442a917433`（64 hex），并由独立解码得到 1,944 ms、PCM RMS 4,305、24 kHz、单声道。NAS predeployment identity 与隔离 synthetic probe JSON 中的 `audio_sha256` 均为 65 位手工转录值，标记为 `INVALID_TRANSCRIPTION`；不输出该字面值、不猜删字符修复，也不据此断言两次探针的音频字节相同。只读更正记录确认原 JSON 保留、没有猜测性修正，并指定 postdeployment 自动探针为有效服务音频证据。隔离 probe 仅保留非身份化观察：合成输入、11,664 bytes、17 个 chunks、运行 service 未修改；原音频未持久化，不能作音频 hash 身份证据。JSON 文件本身的实际 SHA-256 有效并见下方证据索引。探针文本为合成内容，不含儿童内容。

部署后 image 的 NAS service probe 证明当前 service/provider 能生成并解码该音频；OTA 200 只说明 HTTP 可达。探针不证明 S-17 固件已收到、播放或让用户听到答案，也不证明完整 Wake→Speech→ASR→LLM→TTS→Playback→Next Wake 语音轮次。

S-17 的首次失败窗口保留为历史记录：当时只读检查观察到 Edge TTS 7.2.6 在所查 30 分钟 Docker 日志窗口有 25 条 HTTP 403 warning，用户未听到答案，结论为 `WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403`。NAS TTS 服务阻断现已由本次 service 证据解除；该历史窗口不被改写，也不自动转成 DEVICE PASS。

Root 已在未重刷 S-17 app/model 的前提下完成 `preflight-02` 捕获：600.014 秒按 `max_seconds` 正常结束，无串口错误，随后已释放 COM7。用户报告有响应但较慢且回答不正确，实际听到内容及主观延迟没有进一步描述。设备窗口观察到 0 个新 HTTP 403、3 次 NAS interrupt。安全时间线含受控问题 ASR marker（439,032 ms）、首个后续 TTS marker（445,914 ms）、3 个额外 ASR marker，以及 494,463 ms 匹配预期答案文本的 TTS marker；状态时间线记录 501,267 ms speaking→listening、556,615 ms listening→speaking，未见 next-wake marker。这些文本/状态 marker 不是逐帧音频或实际播放证据，AEC、near-end 和播放仍未验证。NAS provider latency 探针未得到可用结果：native Ollama probe exit 1、result null 且错误详情未保留；先前 production-provider 请求在 90 秒总超时后 exit 124，只能说明请求未完成，不能据此推断首字延迟或 `think=false` 对照结果。NAS 同一 image 仍运行、restart count 0。当前设备结果 `PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE`，completed rounds `0/2–3`；正式连续 20 轮未开始，M1 未验收。S18 诊断审计与后续 NAS provider 探针仍在进行，根因未定。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`；Candidate 之间不转移设备结论。

本次 capture 安全 JSON 证据的 SHA-256：`preflight-02-state.json` `ee3128e0cd1123eac0a6bffc1f663cb8836e52a880d2ac6fe3e8baf55e7ba0b7`；`preflight-02-summary.json` `6a18357db3b4328a39f869a12981f84af798964ae55849d58518099f3837d421`；`preflight-02-safe-timeline.json` `38fd15c6b7b9a356a8ee719485fc9ea1bb3e4a9fe1807e877b88a247ddd1c352`；`preflight-02-echo-screen.json` `fc388e7cf2e8305317ca69fd4be3fdd352e69d1a917b4f4360ae00e236575633`；`preflight-02-nas-safe-events.json` `12453d776983cd8383c23ba99a7dbd98f2547485a417c07d0db55ce28aae9502`；`preflight-02-environment.json` `2a1731e2577ae3e37200dea05ecaf57d43df4d8869a57238e483745fef55903c`；`preflight-02-operator-events.jsonl` `42bb95ae483e255a56ce2533d1e329e628dd1efcd5180ffb713cdee650ddc2e7`。不引用会与设备报告互相循环的文档哈希。

## 私有证据索引

原始文件留在 `E:/v6/s17-device-20261018-01/` 私有目录；以下仅记录允许披露的 JSON 摘要和实际文件 SHA-256，不复制原始日志、语音或凭据。

| 文件 | SHA-256 |
| --- | --- |
| `nas-diagnostic-summary.json` | `ebd7346496d37613922395d0f89a078f87e7af2feddcbe17f2320dbdf41cbcc1` |
| `nas-tts-isolated-731-probe.json` | `8d759e698d5ab13ec141a8785cd7fb5cc33dca54e02f8a9d79082f4a485b3be2` |
| `nas-tts-predeployment-identity.json` | `11fd2bfb864bdca66d8e15dcec7d8d467f29cc2f5c378f960ef0c93f773818a8` |
| `nas-tts-postdeployment-identity.json` | `42e4d9f08f09c8f58ac20a6011540de35b75fa6f9337e464b1896657c4713125` |
| `nas-evidence-integrity-correction.json` | `7911fbed78f677c4fc6bef54a971fa31123122cb4fd1c78531d06b7641eaee1a` |
| S-17 device report `V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md` (base `291a4ca` version) | `f1430055d7f6e622e57399e085fec181400e34d11628222c88106235a45e9195` |
| `ollama-latency-native-probe.json` | `f8789d183a6982b6dd37fd32e34e81947738c7298defd73d624d0751b74e2f04` |

L-17 build inputs recorded in the predeployment identity: `Dockerfile.edge-tts` SHA-256 `192baca585fb8d1cdc945d0e14d13336644cc8ab255f4bc3c0169641420228ea`; `edge-tts.requirements.txt` `259ba8659735e984f2a8d5e7d8c6708e926338e7bc66ee51ebaf42397f40e87e`; `.dockerignore` `8cc8586b9cd7ee1c2cdc853b10406d407c49cdf8c4d47cca61acd0a14dcbea74`; `compose.edge-tts-fix.yaml` `9aa3c1a9fdadfe018fe273d9320d974a516b56711ffdb62575dfa846a76cf612`. Each SHA-256 is a full 64-character hex digest.

## 验收状态

- `L-17 source`: `812b12905cda01843f5aeb2d80835abb9bc2b084`；范围仅为 NAS TTS repair implementation。
- `R-11`: `PASS`。
- `NAS_TTS_SERVICE_REPAIR`: `PASS`，基于 hash-identified image、未变化的 provider/config identity 及成功的合成 service probe。
- `DEVICE_PLAYBACK`: `NOT_VERIFIED`；`preflight-02` 结束但有缺口，用户报告响应慢且不正确，completed rounds `0/2–3`；状态 `PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE`。
- 正式 20 轮：未开始。`M1=NOT_ACCEPTED`；`M0=CHANGES_REQUIRED`；AP recovery `SKIPPED_BY_USER / NOT_VERIFIED`。
