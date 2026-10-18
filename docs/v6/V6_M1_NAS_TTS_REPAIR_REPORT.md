# L-18 NAS Edge TTS repair evidence report

记录日期：2026-10-18。此报告补充 S-17 首次 Wake/提问后未听到答案所发现的 NAS TTS 403 阻断。结论限于 `NAS_TTS_SERVICE_REPAIR=PASS`；不宣称设备已播放答案、Preflight 完成或 M1 验收通过。

## 变更与服务身份

L-17（source commit `812b12905cda01843f5aeb2d80835abb9bc2b084`）基于原始 XiaoZhi v0.9.6 digest 固定镜像构建唯一的本地修复镜像 `claw4-xiaozhi-v6:tts-731-20261018-01`，image ID 为 `sha256:acf7314b488ad7e49232797fb42711faad637c00d29142ec782124a92b1d9291`。独立审查 R-11 为 `PASS`。候选保留原 base `ghcr.io/xinnan-tech/xiaozhi-esp32-server:server_0.9.6@sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`，只改变 `edge-tts`：从 7.2.6 升至 7.3.1；未升级其他依赖。

7.3.1 wheel SHA-256 为 `2a3c79d6235a7a736d681233823ce5492e829b43eb47beae49aa181237a8f9a4`。上游 [PR #492](https://github.com/rany2/edge-tts/pull/492) 增加 Sec-MS-GEC v2 token 支持，并记录了相关 403 handshake 修复；[7.3.1 release notes](https://github.com/rany2/edge-tts/releases/tag/7.3.1) 记录 locale-dependent server-date parsing 修复。此次本地结果由下述 NAS/provider 探针支持，不将上游变更描述为完整设备语音修复。

NAS rollout 后服务运行于该本地 image ID，restart count 为 0，OTA HTTP 状态为 200。候选 merged Compose 与原服务配置的差异限于 image/build：merged Compose SHA-256 为 `ffcdb55a0f23e7cf8927684919b751d008d9565a047971d988be122adbf07d0b`；NAS-local config SHA-256 前后均为 `3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438`；原 `compose.yaml` SHA-256 保持 `8e420b89d0381608b5833078168c53c3c41b741fc9f0e971f2630c6d3d488f91`。因此 TTS provider/voice 配置、ports、volumes 和其余 service 配置保持原身份。旧 image `sha256:d8ab77bb1f2b212ee3f1d45fb9d9f9faf953be46c13c70ff6537044dd0b1e27e` 仍保留作回退目标；没有执行 rollback。回退命令见 [NAS deployment README](../../deploy/v6/xiaozhi-nas/README.md)，仅使用原 Compose 文件重建 `xiaozhi` 服务。

## 合成探针与判定边界

唯一用于判定 `NAS_TTS_SERVICE_REPAIR=PASS` 的音频证据是 postdeployment identity 中运行中 NAS service/provider probe：它报告生成 11,664 bytes，实际音频 SHA-256 为 `6cd8ac8ecf1395b976c0dd170c1ccceb8428311253513f0b84c1eb442a917433`（64 hex），并由独立解码得到 1,944 ms、PCM RMS 4,305、24 kHz、单声道。NAS predeployment identity 与隔离 synthetic probe JSON 中的 `audio_sha256` 均为 65 位手工转录值，标记为 `INVALID_TRANSCRIPTION`；不输出该字面值、不猜删字符修复，也不据此断言两次探针的音频字节相同。只读更正记录确认原 JSON 保留、没有猜测性修正，并指定 postdeployment 自动探针为有效服务音频证据。隔离 probe 仅保留非身份化观察：合成输入、11,664 bytes、17 个 chunks、运行 service 未修改；原音频未持久化，不能作音频 hash 身份证据。JSON 文件本身的实际 SHA-256 有效并见下方证据索引。探针文本为合成内容，不含儿童内容。

部署后 image 的 NAS service probe 证明当前 service/provider 能生成并解码该音频；OTA 200 只说明 HTTP 可达。探针不证明 S-17 固件已收到、播放或让用户听到答案，也不证明完整 Wake→Speech→ASR→LLM→TTS→Playback→Next Wake 语音轮次。

S-17 的首次失败窗口保留为历史记录：当时只读检查观察到 Edge TTS 7.2.6 在所查 30 分钟 Docker 日志窗口有 25 条 HTTP 403 warning，用户未听到答案，结论为 `WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403`。NAS TTS 服务阻断现已由本次 service 证据解除；该历史窗口不被改写，也不自动转成 DEVICE PASS。

Root 已在未重刷 S-17 固件的前提下开始新的 `preflight-02` 捕获；结果尚待判定，当前 completed rounds 仍为 `0/2–3`。正式连续 20 轮未开始，M1 未验收。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`；Candidate 之间不转移设备结论。

## 私有证据索引

原始文件留在 `E:/v6/s17-device-20261018-01/` 私有目录；以下仅记录允许披露的 JSON 摘要和实际文件 SHA-256，不复制原始日志、语音或凭据。

| 文件 | SHA-256 |
| --- | --- |
| `nas-diagnostic-summary.json` | `ebd7346496d37613922395d0f89a078f87e7af2feddcbe17f2320dbdf41cbcc1` |
| `nas-tts-isolated-731-probe.json` | `8d759e698d5ab13ec141a8785cd7fb5cc33dca54e02f8a9d79082f4a485b3be2` |
| `nas-tts-predeployment-identity.json` | `11fd2bfb864bdca66d8e15dcec7d8d467f29cc2f5c378f960ef0c93f773818a8` |
| `nas-tts-postdeployment-identity.json` | `42e4d9f08f09c8f58ac20a6011540de35b75fa6f9337e464b1896657c4713125` |
| `nas-evidence-integrity-correction.json` | `7911fbed78f677c4fc6bef54a971fa31123122cb4fd1c78531d06b7641eaee1a` |
| S-17 device report `V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md` | `f1430055d7f6e622e57399e085fec181400e34d11628222c88106235a45e9195` |

L-17 build inputs recorded in the predeployment identity: `Dockerfile.edge-tts` SHA-256 `192baca585fb8d1cdc945d0e14d13336644cc8ab255f4bc3c0169641420228ea`; `edge-tts.requirements.txt` `259ba8659735e984f2a8d5e7d8c6708e926338e7bc66ee51ebaf42397f40e87e`; `.dockerignore` `8cc8586b9cd7ee1c2cdc853b10406d407c49cdf8c4d47cca61acd0a14dcbea74`; `compose.edge-tts-fix.yaml` `9aa3c1a9fdadfe018fe273d9320d974a516b56711ffdb62575dfa846a76cf612`. Each SHA-256 is a full 64-character hex digest.

## 验收状态

- `L-17 source`: `812b12905cda01843f5aeb2d80835abb9bc2b084`；范围仅为 NAS TTS repair implementation。
- `R-11`: `PASS`。
- `NAS_TTS_SERVICE_REPAIR`: `PASS`，基于 hash-identified image、未变化的 provider/config identity 及成功的合成 service probe。
- `DEVICE_PLAYBACK`: `NOT_VERIFIED`；`preflight-02` 已开始但结果待定，completed rounds `0/2–3`。
- 正式 20 轮：未开始。`M1=NOT_ACCEPTED`；`M0=CHANGES_REQUIRED`；AP recovery `SKIPPED_BY_USER / NOT_VERIFIED`。
