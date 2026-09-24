# V6-M1 NAS Voice 验证方案

状态：`IN_PROGRESS (HOST PREPARATION ONLY)`。M0 仍 `CHANGES_REQUIRED`。用户于 2026-09-24 明确要求跳过 Candidate20 AP outage/recovery 刺激并直接开始下一阶段开发；该例外只涵盖主机/工具开发与服务预检，不代表 M0 PASS 或 M1 20 轮验证开始/通过。目前未部署服务、未配置用户凭据、未验证 NAS 连接或 CPU 架构。

服务候选暂定 `xinnan-tech/xiaozhi-esp32-server` v0.9.6，tag commit `f5ed1aaec88471ba00ac778045331514066d63dc`，server image digest `sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`。这是可重复部署的候选 pin，仍待 NAS 架构核对后冻结：该 release 自带的单服务 Docker 部署说明只支持 x86；ARM64 需要本地构建专用镜像。当前 NAS CPU/内存/存储和地址未知，不在 NAS 上拉取、部署或写入任何内容。上游单服务配置还要求模型文件与 `.config.yaml`；ASR/LLM/TTS 的 provider、各自版本和配置 schema 必须从该 tag 的配置核对并冻结，不能拿浮动 `latest` 或空值当成已配置。

上游核对输入：[`v0.9.6` release](https://github.com/xinnan-tech/xiaozhi-esp32-server/releases/tag/v0.9.6)、[tagged single-server deployment guide](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/v0.9.6/docs/Deployment.md)、[tagged Compose](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/v0.9.6/main/xiaozhi-server/docker-compose.yml)、[GHCR package digest](https://github.com/xinnan-tech/xiaozhi-esp32-server/pkgs/container/xiaozhi-esp32-server)。NAS 地址、资源与模型/ASR/TTS 可用性核验仍是部署前置条件。密钥置本地环境或密钥存储，报告只记录脱敏标识。先用合成内容，默认不上传儿童数据。

设备仅运行官方 Voice Core，经配置连接 NAS；没有 Learning UI/命令路由补丁。记录协议版本、鉴权和时间同步，确认关闭不需要的工具。仅完成网络连接不算语音通过。

## 20 轮连续流程

`M1-DEV-01` 已由 Astra 验收（Sol 最终修复提交 `75d0059a1f0ac6380c1e1e3ee5f41125a44139f2`；base `db97ad5994f3ee0c5e1fb4deb2fa4fa2ec3ee1ee`）。校验器绑定 reviewed source/app/ELF SHA、server commit/image digest、config SHA、ASR/LLM/TTS 版本、同一会话下恰好 20 个有序轮次，并要求每轮 synthetic stimulus ID、分域时延、资源数值和故障测试结果；PASS 只表示证据结构齐备且所有故障项显式 PASS，不代表语音质量或 M1 已验收。验证：定向 13/13、V6 tools 109/109。运行方式：`py -3.14 -B tools/v6/m1_round_evidence.py input.json output.json`。工具不连接设备、服务或 NAS，也不输出刺激 ID/原始文本。

同一候选、同一服务版本，唤醒→学生说话→ASR→LLM→TTS→下一轮，至少连续 20 轮。不重启、不手工逐轮恢复服务。用合成问句覆盖短句、长句、静默、打断；验证 TTS 播放期间与结束后不会触发自我对话。

每轮记录：轮次、ASR final 延迟、LLM first-token、TTS first-audio、设备可听首响、是否误回采/卡住、heap/PSRAM 最小余量。设备与服务指标使用各自单调时钟；未同步时钟不能直接跨机相减，未获取指标明确 N/A，不能填 0。延迟先报告 p50/p95/max，不能无依据设定“全部正常”。

补充故障：NAS 断开/恢复、WebSocket 重连、TTS 中途断网、静默超时、唤醒后无 ASR、反复打断。记录状态回归与资源趋势。WDT、崩溃、自动重启、无限回采、说一句后失去响应任何一项发生即 CHANGES_REQUIRED，修复后重新跑连续 20 轮。

报告：代码/配置 digest、构建证据、设备验证、脱敏日志、重复步骤五项齐备后才能放行 M1.5。M1.5 再定位 ASR final → intent → LLM 分岔 hook，验证命令匹配时普通 LLM 调用次数为 0；本阶段不提前加学习命令。
