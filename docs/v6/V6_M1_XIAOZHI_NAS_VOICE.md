# V6-M1 NAS Voice 验证方案

状态：`SERVICE_DEPLOYED / DEVICE_VOICE_NOT_VERIFIED`。M0 仍 `CHANGES_REQUIRED`。用户于 2026-09-24 明确要求跳过 Candidate20 AP outage/recovery 刺激并直接开始下一阶段开发，随后明确授权在铭凡 N5 x86 NAS 上部署小智服务。该进展不代表 M0 PASS 或 M1 20 轮验证通过。部署与合成连通性证据见 [NAS 部署报告](V6_M1_NAS_DEPLOYMENT_REPORT.md)；未配置外部提供商凭据，未使用儿童数据。

服务固定为 `xinnan-tech/xiaozhi-esp32-server` v0.9.6，tag commit `f5ed1aaec88471ba00ac778045331514066d63dc`，server image digest `sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`。NAS 已确认 `x86_64`、60 GiB 内存，部署在用户指定的 `/vol2/1008/docker/xiaozhi-v6`。已核对并固定模型文件、`.config.yaml`、本地 ASR/Ollama LLM 与 EdgeTTS 选择；镜像不使用浮动 `latest`。完整文件哈希、端口、组件与局域网连通性结果见部署报告。

上游核对输入：[`v0.9.6` release](https://github.com/xinnan-tech/xiaozhi-esp32-server/releases/tag/v0.9.6)、[tagged single-server deployment guide](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/v0.9.6/docs/Deployment.md)、[tagged Compose](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/v0.9.6/main/xiaozhi-server/docker-compose.yml)、[GHCR package digest](https://github.com/xinnan-tech/xiaozhi-esp32-server/pkgs/container/xiaozhi-esp32-server)。密钥只允许置 NAS 本地环境或密钥存储，报告只记录脱敏标识。当前仅用合成内容，不上传儿童数据。

设备仅运行官方 Voice Core，经配置连接 NAS；没有 Learning UI/命令路由补丁。记录协议版本、鉴权和时间同步，确认关闭不需要的工具。仅完成网络连接不算语音通过。

## 20 轮连续流程

`M1-DEV-01` 已由 Astra 验收（Sol 最终修复提交 `75d0059a1f0ac6380c1e1e3ee5f41125a44139f2`；base `db97ad5994f3ee0c5e1fb4deb2fa4fa2ec3ee1ee`）。校验器绑定 reviewed source/app/ELF SHA、server commit/image digest、config SHA、ASR/LLM/TTS 版本、同一会话下恰好 20 个有序轮次，并要求每轮 synthetic stimulus ID、分域时延、资源数值和故障测试结果；PASS 只表示证据结构齐备且所有故障项显式 PASS，不代表语音质量或 M1 已验收。验证：定向 13/13、V6 tools 109/109。运行方式：`py -3.14 -B tools/v6/m1_round_evidence.py input.json output.json`。工具不连接设备、服务或 NAS，也不输出刺激 ID/原始文本。

同一候选、同一服务版本，唤醒→学生说话→ASR→LLM→TTS→下一轮，至少连续 20 轮。不重启、不手工逐轮恢复服务。用合成问句覆盖短句、长句、静默、打断；验证 TTS 播放期间与结束后不会触发自我对话。

每轮记录：轮次、ASR final 延迟、LLM first-token、TTS first-audio、设备可听首响、是否误回采/卡住、heap/PSRAM 最小余量。设备与服务指标使用各自单调时钟；未同步时钟不能直接跨机相减，未获取指标明确 N/A，不能填 0。延迟先报告 p50/p95/max，不能无依据设定“全部正常”。

补充故障：NAS 断开/恢复、WebSocket 重连、TTS 中途断网、静默超时、唤醒后无 ASR、反复打断。记录状态回归与资源趋势。WDT、崩溃、自动重启、无限回采、说一句后失去响应任何一项发生即 CHANGES_REQUIRED，修复后重新跑连续 20 轮。

报告：代码/配置 digest、构建证据、设备验证、脱敏日志、重复步骤五项齐备后才能放行 M1.5。M1.5 再定位 ASR final → intent → LLM 分岔 hook，验证命令匹配时普通 LLM 调用次数为 0；本阶段不提前加学习命令。
