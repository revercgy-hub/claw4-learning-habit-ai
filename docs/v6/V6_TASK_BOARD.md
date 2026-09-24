# V6 唯一阶段看板

> **SUPERSEDED 历史快照（2026-09-23）**：Candidate19 已构建但未刷机；Candidate17 DEVICE 观察仅属于 Candidate17。Candidate19 没有 DEVICE PASS。旧工具测试统计按发生时间分别为 C19 构建时 68 项、接管基线 83 项（增加 15 项）；均为 Host 工具测试，不是设备验收。AEC NOT PASS；旧 `clipped=0` 计数无效。

## 2026-09-24 接管实施状态（当前有效）

接管基线 `760b2aa66819f8d90186b43af4c02d9b33c8849e` 已保护至 `origin/takeover-v6-m0-c19-baseline`；A-01 已推送至 `origin/codex/takeover-v6-m0-integration` @ `7d55060`。Candidate19 built, not flashed。`M0=IN_PROGRESS`、接管审查首轮 `CHANGES_REQUIRED` 且定向复审待完成、`M1=BACKLOG`。CODE、HOST、BUILD、DEVICE 是独立证据类别；Candidate17 的设备 PASS 不得转移至 Candidate19；旧 `clipped` 计数不用于验收。

第一实施波 L-01、L-02、S-01、S-02 均已提交并整合至 `7263f67`；L-02 定向复核修复整合至 `a128341`。第一波现为 `REVIEW_READY`，等待 R-01 定向复审。R-01 首轮为 `CHANGES_REQUIRED`，修复已进入复审；当前结论仍未 PASS。

| 队列 | 状态 | 说明 |
| --- | --- | --- |
| L-01 / L-02 / S-01 / S-02 | REVIEW_READY | 四项均已提交并整合；L-02 定向复核修复已整合；待 R-01 定向复审 |
| R-01 | REVIEW_READY | 首轮结论 CHANGES_REQUIRED；目标修复已整合，定向复审待完成；不得标 PASS |
| S-03 | QUEUED / 未启动 | 仅 R-01 PASS 后由单一 owner 串行执行 Build、身份核验、刷写、读回/证据和设备矩阵 |
| A-02 | QUEUED | S-03 后核验同一候选的 CODE/HOST/BUILD/DEVICE 与 recovery evidence；不满足则保留 CHANGES_REQUIRED |

M0 必需项仍有未闭合项，故 `M0=IN_PROGRESS`；M1 保持 `BACKLOG`。M1 定义为 NAS 连续语音 20 轮。旧 relay 学习闭环属于旧阶段设计，不能视作 M1 已完成或 M1 验收范围。旧 Candidate05–08 与历史看板中的 READY 入口一律 `SUPERSEDED`，只保留为历史记录，不构成当前领取授权。

以下未再次标注的旧看板内容均为历史快照（SUPERSEDED），其中遗留的“当前有效”、READY 或派发入口只保留历史证据，不构成现行队列。

| 顺序 | 工作 | 负责人 | 状态 | 放行与证据 |
| --- | --- | --- | --- | --- |
| 0 | 架构/迁移构建入口/边界检查 | Codex | REVIEW_READY | 代码/文档已交付；78 case PASS，协调器受应用控制阻断，不能标全量 PASS |
| M0-1 | 四方源码冻结 + 环境/恢复清单 | Codex | IN_PROGRESS | SHA/IDF6.1/依赖锁已冻结；32MiB 完整备份和实际布局已核对，恢复写回尚未实测 |
| M0-2 | Claw4 Board Port | Codex | IN_PROGRESS | SD 挂载、metadata-only camera sensor init、触摸与电源键短/长按识别已实机确认；Candidate18 的限时单帧 RAW8 诊断已构建，尚未刷机验证。硬件矩阵解析已覆盖 SD 单次挂载、电源键短/长按和相机单帧/清理结果（接管基线共 83 项 Host 工具测试；C19 构建时为 68 项，之后新增 15 项；不代表新增实机验证）。电源键产品动作未实现。物理 RX ch1 保持静默；诊断版软件参考可进入 AFE ch1，但 AEC 效果未通过 |
| M0-NET | 隐藏网络回退与可复现依赖补丁 | Codex | REVIEW_READY | f47afda；62工具测试、7生产方法Host场景、IDF构建通过；真机未验证 |
| M0-NET-ERR | 网络失败恢复增量 | Codex | REVIEW_READY | 候选10编译链接通过，正常隐藏网络回退及取IP实测；同步调用失败分支未故障注入 |
| M0-3 | Candidate13 回归与音频闭环 | Codex | IN_PROGRESS | Candidate14/15 证实软件参考注入至 AFE ch1；90ms 与 0ms 两组各两次播放结果不一致，且非配对测试，AEC NOT PASS。Candidate17 五分钟静置采样无复位，读取失败/参考丢帧为 0；旧 clipped 字段不具削波证据，Candidate19 的 near_full_scale_n 已构建、未刷机。交互/AEC/播放期 WakeNet 仍待复测。Candidate18 单帧相机取帧等待实机验证；还需设备网络同步失败注入与完整长稳 |
| M1 | NAS 连续语音 20 轮 | Codex | BACKLOG | M0 PASS 后再启；旧 relay 学习闭环属于旧阶段，不计入 M1 |
| M1.5 | 七命令 pre-LLM router | Codex | BACKLOG | M1；认证/幂等/结果确认契约 |
| M2 | 真实学习闭环 | Codex；可拆 WorkBuddy 辅助 | BACKLOG | M1.5；在线/离线/重启/补传 |
| M2.5 | 离线主动提醒 MVP | Codex | BACKLOG | M2；RTC/TimeAuthority、缓存/内置音频恢复 |
| P0 总验收 | V6 原方案 §10 十一步 | Codex | BACKLOG | 20 轮 + 命令 + 断 NAS 提醒 + 完成 + PWA |
| M3 | Camera/WrongBook 业务 | 待分配 | HOLD | P0 总验收通过 |
| M4 | Learning Memory | 待分配 | HOLD | M3；事实与对话记忆独立 |

历史辅助任务建议（SUPERSEDED，不构成当前授权）：冻结 Board 接口后补硬件矩阵记录模板与重跑脚本；冻结 NAS schema 后补合成集成用例。尚未 READY，不能自行修改 Board/Voice 架构或沿 V5.3 继续接线。实际下发时另给允许路径、Base SHA、独立分支、测试、停止条件，Codex 按不可变提交复核。
