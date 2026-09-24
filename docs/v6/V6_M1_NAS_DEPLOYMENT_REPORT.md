# V6 M1 XiaoZhi NAS 部署报告

状态：`SERVICE_DEPLOYED / CONNECTIVITY_PASS / DEVICE_VOICE_NOT_VERIFIED`。日期：2026-09-24。M0 仍 `CHANGES_REQUIRED`；用户跳过的 Candidate20 AP outage/recovery 保持 `SKIPPED_BY_USER / NOT_VERIFIED`。本报告不宣称 M1 20 轮语音通过。

## 目标与固定身份

- 主机：铭凡 N5，`x86_64`，`192.168.3.100`，Docker Compose `2.40.3`；部署目录按用户指定为 `/vol2/1008/docker/xiaozhi-v6`。未改动已有容器或其 Compose 项目。
- 上游：`xinnan-tech/xiaozhi-esp32-server` `v0.9.6`，tag commit `f5ed1aaec88471ba00ac778045331514066d63dc`。
- 镜像：`ghcr.io/xinnan-tech/xiaozhi-esp32-server:server_0.9.6@sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`；NAS 验证 `linux/amd64`，image ID `sha256:d8ab77bb1f2b212ee3f1d45fb9d9f9faf953be46c13c70ff6537044dd0b1e27e`。
- NAS Compose SHA-256：`8e420b89d0381608b5833078168c53c3c41b741fc9f0e971f2630c6d3d488f91`；NAS `data/.config.yaml` SHA-256：`3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438`，与仓库模板逐字节一致。配置不含凭据。应用户要求，当前宿主端口为 WebSocket `7444 → 8000`、HTTP/OTA `7443 → 8003`；7443 仍是明文 HTTP，未添加 TLS。
- ASR：本地 SenseVoiceSmall `model.pt`，936,291,369 字节，SHA-256 `833ca2dcfdf8ec91bd4f31cfac36d6124e0c459074d5e909aec9cabe6204a3ea`。从 ModelScope 下载并与 [官方模型文件记录](https://huggingface.co/FunAudioLLM/SenseVoiceSmall/commit/2b05887414d06ebf81c0256c82b295450dcfc765) 比对。
- LLM：NAS 现有 Ollama `0.30.7` / `qwen3.5:2b`，模型 digest `324d162be6ca5629ae4517c8710434d0bd2d665bc94dbad46e9af8fbf8a2f0ddf`，经容器 `host.docker.internal:11434` 访问。VAD `SileroVAD` (`silero-vad` 6.1.0)、ASR `FunASR` (1.2.7)、TTS `EdgeTTS` (`edge-tts` 7.2.6)、Memory `nomem`、Intent `nointent`。EdgeTTS 是外部服务，目前只发送合成测试文字，不上传儿童数据。

## 实际验证

1. 拉取固定 digest 成功；`docker compose config --quiet` 通过；`docker compose up -d --no-build --pull never` 启动。容器 `claw4-xiaozhi-v6` 后续检查为 `running`、`restarts=0`；稳态采样约 2.925 GiB 内存。
2. 启动日志报告 `OllamaLLM`、`nointent`、`nomem`、`SileroVAD`、`FunASR` 初始化成功（FunASR 1.2.7）。日志打印容器内部 IP 属上游行为，实际设备端点以 OTA 返回值为准。
3. NAS 和工作机访问 `http://192.168.3.100:7443/xiaozhi/ota/` 均返回 HTTP 200，内容给出 `ws://192.168.3.100:7444/xiaozhi/v1/`；工作机对新 WebSocket 端口的 Upgrade 返回 `101 Switching Protocols`。NAS 监听仅有新端口 7443/7444，旧 8000/8003 宿主监听已关闭；重建后容器 `running`、`restarts=0`。
4. 容器内访问 NAS Ollama `/api/tags` 成功，含 `qwen3.5:2b`；从容器发起的合成文字生成请求返回 `done=True` 与预期文本。容器内 EdgeTTS 合成测试生成 20,448 字节音频。

以上是部署、网络和各组件的合成连通性证据，不是设备到 ASR→LLM→TTS 的完整语音轮次，也不是音质或时延验收。未操作 COM7、Claw4、Flash、`E:/v6/s1` 或 NVS recovery。

初始部署时宿主端口 8000/8003 的连通性观察已由 7444/7443 验证取代，状态 `SUPERSEDED`；对应旧 Compose SHA-256 为 `84182bcc943613211a2e121eac347f25e8a114a4cdbf273a818732372e4fcf43`、旧覆盖配置 SHA-256 为 `a2a67e21592c755d1cdadc5cf2af42d981805843f59ab03193c95cdf709b55fe`。模型、镜像及服务内部端口未变。

## 后续缺口

- 用同一设备 Candidate 与上述固定服务配置，执行 [M1 方案](V6_M1_XIAOZHI_NAS_VOICE.md) 的连续 20 轮及故障矩阵，记录 CODE/HOST/BUILD/DEVICE 分域证据和配置/日志会话绑定。
- EdgeTTS 外部服务目前仅以合成内容验证。正式真实语音使用前需按项目隐私约束审定输入范围；本报告不推定儿童数据上传许可。
- M0 Gate 仍按 [A-02](V6_M0_CANDIDATE20_GATE.md) 的剩余缺口处理，不能因 NAS 服务可用而转为 PASS。
