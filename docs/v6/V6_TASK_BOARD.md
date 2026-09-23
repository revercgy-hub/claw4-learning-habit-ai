# V6 唯一阶段看板

> 当前有效（2026-09-23）：Candidate13 已完成 IDF 构建、62 项 Python、19 个网络 Host 场景和 C++ 测试；app-only 写入/回读 SHA256 一致。多段串口测试确认冷启动、SD 挂载、camera sensor init、触摸录放、Wi-Fi 最终连通、电源键短/长按识别；4 次低音量唤醒词有 2 次命中且均重新布防。新发现 AEC 缺陷：安静回放期间 VAD 仍触发两次，输入参考 ch1 在全部 113 个窗口全零。详见 `V6_M0_CANDIDATE13_REPORT.md`。M0 未验收，AEC/参考链路修复、网络同步失败故障注入、长稳和相机取帧仍未闭合；M1 BACKLOG。

> 当前状态（2026-09-22阶段收口）：用户要求的 M0 网络恢复与统一候选阶段已完成源码/Host/BUILD，提交 f47afda。唯一 WorkBuddy 活动流 **WB-V6-M0-CANDIDATE07-REVIEW / READY**，任务入口 docs/project_management/tasks/WB-V6-M0-CANDIDATE07-REVIEW.md；阶段报告 docs/v6/V6_M0_NETWORK_STAGE_REPORT.md。候选07仅构建冻结、未刷机，先独立代码复核再按包测试。06包HOLD，旧05/06 READY文字均为SUPERSEDED历史。M0整体与M1门禁不变；任务包仅本地发布，未外部发送。

> 2026-09-22 最新覆盖：candidate05 @ bf34b4e 已复核，CHANGES_REQUIRED，修订由 candidate06 承接。唯一 WorkBuddy 活动流 **WB-V6-M0-CANDIDATE06-TEST / READY**；任务包 `docs/project_management/tasks/WB-V6-M0-CANDIDATE06-TEST.md`，复核 `docs/v6/CODEX_V6_CANDIDATE05_REVIEW_AND_06.md`。候选06已构建、56工具测试及C++测试通过，尚未刷机；设备测试由WorkBuddy独占执行。本地任务包已发布，未通过外部消息工具送达。下文05调度状态 SUPERSEDED，M0/M1门禁不变。

更新：2026-09-22。负责人：Codex。当前开发 CODEX-V6-FOUNDATION / IN_PROGRESS；唯一 WorkBuddy 活动流 WB-V6-M0-CANDIDATE05-TEST / READY，任务包 ../project_management/tasks/WB-V6-M0-CANDIDATE05-TEST.md。用户要求后续测试交 WorkBuddy，Codex 负责架构/固件修正与复核。候选 05 已构建刷入；Codex 完整记账 10/20 轮正常复位，已停止并释放串口，后续以独立测试流为准。任务包已发布在本地仓库，尚未通过外部消息工具送达 WorkBuddy。


| 顺序 | 工作 | 负责人 | 状态 | 放行与证据 |
| --- | --- | --- | --- | --- |
| 0 | 架构/迁移构建入口/边界检查 | Codex | REVIEW_READY | 代码/文档已交付；78 case PASS，协调器受应用控制阻断，不能标全量 PASS |
| M0-1 | 四方源码冻结 + 环境/恢复清单 | Codex | IN_PROGRESS | SHA/IDF6.1/依赖锁已冻结；32MiB 完整备份和实际布局已核对，恢复写回尚未实测 |
| M0-2 | Claw4 Board Port | Codex | IN_PROGRESS | SD 挂载、metadata-only camera sensor init、触摸与侧键短/长按识别已实机确认；相机未取帧，电源键产品动作未实现。AEC 输入参考 ch1 实测全零，需修复或安全降级 |
| M0-NET | 隐藏网络回退与可复现依赖补丁 | Codex | REVIEW_READY | f47afda；62工具测试、7生产方法Host场景、IDF构建通过；真机未验证 |
| M0-NET-ERR | 网络失败恢复增量 | Codex | REVIEW_READY | 候选10编译链接通过，正常隐藏网络回退及取IP实测；同步调用失败分支未故障注入 |
| M0-3 | Candidate13 回归与音频闭环 | Codex | IN_PROGRESS | 唤醒词4次低音量说话命中2次，两次后均rearm；网络最终取得IP，短/长按识别通过；但安静回放触发2次VAD且AEC参考ch1全零。录放可闻已用户确认；AEC修复、音量余量、同步重连故障注入、长稳未闭合 |
| M1 | NAS 连续语音 20 轮 | Codex | BACKLOG | M0 完成后再启；当前不链接学习代码 |
| M1.5 | 七命令 pre-LLM router | Codex | BACKLOG | M1；认证/幂等/结果确认契约 |
| M2 | 真实学习闭环 | Codex；可拆 WorkBuddy 辅助 | BACKLOG | M1.5；在线/离线/重启/补传 |
| M2.5 | 离线主动提醒 MVP | Codex | BACKLOG | M2；RTC/TimeAuthority、缓存/内置音频恢复 |
| P0 总验收 | V6 原方案 §10 十一步 | Codex | BACKLOG | 20 轮 + 命令 + 断 NAS 提醒 + 完成 + PWA |
| M3 | Camera/WrongBook 业务 | 待分配 | HOLD | P0 总验收通过 |
| M4 | Learning Memory | 待分配 | HOLD | M3；事实与对话记忆独立 |

WorkBuddy 候选辅助任务：冻结 Board 接口后补硬件矩阵记录模板与重跑脚本；冻结 NAS schema 后补合成集成用例。尚未 READY，不能自行修改 Board/Voice 架构或沿 V5.3 继续接线。实际下发时另给允许路径、Base SHA、独立分支、测试、停止条件，Codex 按不可变提交复核。
