# V6 唯一阶段看板

> **SUPERSEDED 历史快照（2026-09-23）**：Candidate19 已构建但未刷机；Candidate17 DEVICE 观察仅属于 Candidate17。Candidate19 没有 DEVICE PASS。旧工具测试统计按发生时间分别为 C19 构建时 68 项、接管基线 83 项（增加 15 项）；均为 Host 工具测试，不是设备验收。AEC NOT PASS；旧 `clipped=0` 计数无效。

## 2026-09-24 接管与用户指令状态（当前有效）

接管基线 `760b2aa66819f8d90186b43af4c02d9b33c8849e` 已保护至 `origin/takeover-v6-m0-c19-baseline`。Candidate20（`claw4-learning-v6-m0.20`）已构建并仅对 live `ota_0` 做 app-only 刷写；读回与 app hash 一致。候选构建布局仍与 live 分区布局不同，未执行恢复写回。A-02 结论为 `M0=CHANGES_REQUIRED`。

用户于 2026-09-24 明确要求跳过 AP outage/recovery 刺激并直接开始下一阶段开发，随后授权在铭凡 N5 x86 NAS 部署小智服务。该指令不改变 M0 结论、不把 AP failure/recovery 标成通过，也不代表 M1 20 轮验收已完成。NAS 服务已部署并通过合成连通性验证；M1 真机语音轮次尚未开始，不进行新的设备写入或 Flash/recovery 范围扩展。

Candidate20 的 CODE/HOST/BUILD/DEVICE 证据分开记账；Candidate17/18/19 DEVICE 结果不得转移；旧 `clipped` 计数不用于验收。

第一实施波 L-01、L-02、S-01、S-02 均已提交并整合至 `7263f67`；L-02 定向复核修复整合至 `a128341`。独立 R-01 在 `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` 复审通过，Host 结果 96/0。

| 队列 | 状态 | 说明 |
| --- | --- | --- |
| L-01 / L-02 / S-01 / S-02 | ACCEPTED | CODE/HOST 经 R-01 通过；DEVICE 证据仍按 Candidate20 单独闭合 |
| R-01 | PASS | 独立复审 source `a164f11ab5f5843d6ace5c4b990d86a29195a2fe`；Host 96/0 |
| S-03 | COMPLETE_WITH_GAPS | app-only readback PASS；设备矩阵部分观察；AP recovery 为 `SKIPPED_BY_USER / NOT_VERIFIED`；详见 [S-03 报告](V6_M0_CANDIDATE20_DEVICE_REPORT.md) |
| A-02 | CHANGES_REQUIRED | Camera 抓帧失败，AEC/唤醒、网络恢复、冷启动、稳定性、故障注入和 recovery evidence 未闭合；布局仍不匹配；见 [Gate 报告](V6_M0_CANDIDATE20_GATE.md) |
| M1-DEV-01 | ACCEPTED | Sol 定向收口；base `db97ad5994f3ee0c5e1fb4deb2fa4fa2ec3ee1ee`；提交 `75d0059a1f0ac6380c1e1e3ee5f41125a44139f2`；定向 13/13、V6 tools 109/109；仅证据工具，不代表 M1 语音验收 |
| M1-NAS-DEPLOY | SERVICE_DEPLOYED | 固定 v0.9.6 镜像、SenseVoiceSmall、本地 Ollama 和 EdgeTTS；OTA/WS/合成 LLM/TTS 连通性通过；[部署报告](V6_M1_NAS_DEPLOYMENT_REPORT.md)；DEVICE 20 轮 NOT_VERIFIED |
| L-03 | COMPLETE | Luna：根看板已同步；集成于 `bde85a5`，不改变架构或历史证据 |
| L-04 | COMPLETE | Luna：M1 Preflight/20 轮模板、日志解析与运行清单已集成至 `ee1a83b`；不改 Voice 架构或既有证据契约 |
| S-04 | COMPLETE | Sol：两处 NVS 初始化已失效关闭；新 Candidate `claw4-learning-v6-m1-preflight-s04-r02-20260925-02` 的 BUILD/Host/身份已冻结至 `f5343c2`；旧 app/ELF `SUPERSEDED` |
| R-02 | PASS | 独立 Sol reviewer：新 stage/镜像/锁哈希匹配，构建来源与集成代码 21/21 Git blob 相等；Host 120/120，设备仍 `NOT_VERIFIED`；仅放行 S-05 的 2～3 轮 Preflight |
| S-05 | BLOCKED_ENDPOINT_MISMATCH | 设备身份/live 布局与 app-only 写入及独立读回通过；启动后实际访问外部 `api.tenclass.net` HTTPS/MQTT，而非 NAS 7443/7444，语音轮次 0；用户已断电、硬件 owner 已释放；见 [设备报告](V6_M1_VOICE_PREFLIGHT_DEVICE_REPORT.md)。修正端点后须新 Candidate/复审/重核身份；AP outage/recovery 不重派 |

Candidate20 已有有限 DEVICE 日志、会话和 readback 事实，但不满足同候选 M0 完整证据；旧全片备份的分区表不同于当前设备，不能作为当前布局的已验证恢复基线。M0 Gate 保持 `CHANGES_REQUIRED`。M1 主机工具与 NAS 部署/合成连通性已完成；M1 验收仍定义为同一候选、同一服务版本下 NAS 连续语音 20 轮和方案规定的故障测试。旧 relay 学习闭环及 Candidate05–08 的 READY 入口均为 `SUPERSEDED`，不构成当前授权。

以下未再次标注的旧看板内容均为历史快照（SUPERSEDED），其中遗留的“当前有效”、READY 或派发入口只保留历史证据，不构成现行队列。

| 顺序 | 工作 | 负责人 | 状态 | 放行与证据 |
| --- | --- | --- | --- | --- |
| 0 | 架构/迁移构建入口/边界检查 | Codex | REVIEW_READY | 代码/文档已交付；78 case PASS，协调器受应用控制阻断，不能标全量 PASS |
| M0-1 | 四方源码冻结 + 环境/恢复清单 | Codex | IN_PROGRESS | SHA/IDF6.1/依赖锁已冻结；32MiB 完整备份和实际布局已核对，恢复写回尚未实测 |
| M0-2 | Claw4 Board Port | Codex | IN_PROGRESS | SD 挂载、metadata-only camera sensor init、触摸与电源键短/长按识别已实机确认；Candidate18 的限时单帧 RAW8 诊断已构建，尚未刷机验证。硬件矩阵解析已覆盖 SD 单次挂载、电源键短/长按和相机单帧/清理结果（接管基线共 83 项 Host 工具测试；C19 构建时为 68 项，之后新增 15 项；不代表新增实机验证）。电源键产品动作未实现。物理 RX ch1 保持静默；诊断版软件参考可进入 AFE ch1，但 AEC 效果未通过 |
| M0-NET | 隐藏网络回退与可复现依赖补丁 | Codex | REVIEW_READY | f47afda；62工具测试、7生产方法Host场景、IDF构建通过；真机未验证 |
| M0-NET-ERR | 网络失败恢复增量 | Codex | REVIEW_READY | 候选10编译链接通过，正常隐藏网络回退及取IP实测；同步调用失败分支未故障注入 |
| M0-3 | Candidate13 回归与音频闭环 | Codex | IN_PROGRESS | Candidate14/15 证实软件参考注入至 AFE ch1；90ms 与 0ms 两组各两次播放结果不一致，且非配对测试，AEC NOT PASS。Candidate17 五分钟静置采样无复位，读取失败/参考丢帧为 0；旧 clipped 字段不具削波证据，Candidate19 的 near_full_scale_n 已构建、未刷机。交互/AEC/播放期 WakeNet 仍待复测。Candidate18 单帧相机取帧等待实机验证；还需设备网络同步失败注入与完整长稳 |
| M1 | NAS 连续语音 20 轮 | Astra/Sol/Luna 按任务拆分 | CHANGES_REQUIRED (ENDPOINT) | M0 仍 CHANGES_REQUIRED；NAS 合成连通性通过；首个 S-05 设备启动连接外部 HTTPS/MQTT，语音 0 轮，固件端点需修正并重新审查；正式 20 轮未开始 |
| M1.5 | 七命令 pre-LLM router | Codex | BACKLOG | M1；认证/幂等/结果确认契约 |
| M2 | 真实学习闭环 | Codex；可拆 WorkBuddy 辅助 | BACKLOG | M1.5；在线/离线/重启/补传 |
| M2.5 | 离线主动提醒 MVP | Codex | BACKLOG | M2；RTC/TimeAuthority、缓存/内置音频恢复 |
| P0 总验收 | V6 原方案 §10 十一步 | Codex | BACKLOG | 20 轮 + 命令 + 断 NAS 提醒 + 完成 + PWA |
| M3 | Camera/WrongBook 业务 | 待分配 | HOLD | P0 总验收通过 |
| M4 | Learning Memory | 待分配 | HOLD | M3；事实与对话记忆独立 |

历史辅助任务建议（SUPERSEDED，不构成当前授权）：冻结 Board 接口后补硬件矩阵记录模板与重跑脚本；冻结 NAS schema 后补合成集成用例。尚未 READY，不能自行修改 Board/Voice 架构或沿 V5.3 继续接线。实际下发时另给允许路径、Base SHA、独立分支、测试、停止条件，Codex 按不可变提交复核。
