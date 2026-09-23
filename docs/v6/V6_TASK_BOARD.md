# V6 唯一阶段看板

> 当前有效（2026-09-23）：候选10网络修复及候选11板级按键诊断已通过 IDF 构建、62 项 Python 与 19 个网络 Host 场景；候选11 app-only 写入/完整回读哈希一致，`BOOT_READY`、资源应用、按键诊断 armed。短按/长按的真机事件日志仍待操作确认。用户确认说话录放可闻；正常音量余量、真机同步重连故障注入及 AEC 参考仍待验证。证据见 `V6_M0_CANDIDATE10_REPORT.md` 与 `V6_M0_CANDIDATE11_REPORT.md`。M0 未总验收，M1 BACKLOG。

> 当前状态（2026-09-22阶段收口）：用户要求的 M0 网络恢复与统一候选阶段已完成源码/Host/BUILD，提交 f47afda。唯一 WorkBuddy 活动流 **WB-V6-M0-CANDIDATE07-REVIEW / READY**，任务入口 docs/project_management/tasks/WB-V6-M0-CANDIDATE07-REVIEW.md；阶段报告 docs/v6/V6_M0_NETWORK_STAGE_REPORT.md。候选07仅构建冻结、未刷机，先独立代码复核再按包测试。06包HOLD，旧05/06 READY文字均为SUPERSEDED历史。M0整体与M1门禁不变；任务包仅本地发布，未外部发送。

> 2026-09-22 最新覆盖：candidate05 @ bf34b4e 已复核，CHANGES_REQUIRED，修订由 candidate06 承接。唯一 WorkBuddy 活动流 **WB-V6-M0-CANDIDATE06-TEST / READY**；任务包 `docs/project_management/tasks/WB-V6-M0-CANDIDATE06-TEST.md`，复核 `docs/v6/CODEX_V6_CANDIDATE05_REVIEW_AND_06.md`。候选06已构建、56工具测试及C++测试通过，尚未刷机；设备测试由WorkBuddy独占执行。本地任务包已发布，未通过外部消息工具送达。下文05调度状态 SUPERSEDED，M0/M1门禁不变。

更新：2026-09-22。负责人：Codex。当前开发 CODEX-V6-FOUNDATION / IN_PROGRESS；唯一 WorkBuddy 活动流 WB-V6-M0-CANDIDATE05-TEST / READY，任务包 ../project_management/tasks/WB-V6-M0-CANDIDATE05-TEST.md。用户要求后续测试交 WorkBuddy，Codex 负责架构/固件修正与复核。候选 05 已构建刷入；Codex 完整记账 10/20 轮正常复位，已停止并释放串口，后续以独立测试流为准。任务包已发布在本地仓库，尚未通过外部消息工具送达 WorkBuddy。


| 顺序 | 工作 | 负责人 | 状态 | 放行与证据 |
| --- | --- | --- | --- | --- |
| 0 | 架构/迁移构建入口/边界检查 | Codex | REVIEW_READY | 代码/文档已交付；78 case PASS，协调器受应用控制阻断，不能标全量 PASS |
| M0-1 | 四方源码冻结 + 环境/恢复清单 | Codex | IN_PROGRESS | SHA/IDF6.1/依赖锁已冻结；32MiB 完整备份和实际布局已核对，恢复写回尚未实测 |
| M0-2 | Claw4 Board Port | Codex | IN_PROGRESS | 候选11加入只记录、不控电源的 TCA9555 P0_5 短按/长按诊断；待真机事件日志确认；Camera/SD 与产品电源键动作待补 |
| M0-NET | 隐藏网络回退与可复现依赖补丁 | Codex | REVIEW_READY | f47afda；62工具测试、7生产方法Host场景、IDF构建通过；真机未验证 |
| M0-NET-ERR | 网络失败恢复增量 | Codex | REVIEW_READY | 候选10编译链接通过，正常隐藏网络回退及取IP实测；同步调用失败分支未故障注入 |
| M0-3 | 候选08复检与候选10回归 | Codex | IN_PROGRESS | 报告 V6_M0_CANDIDATE10_REPORT.md；基础稳定性引用05，候选08网络差异已测，候选10启动/网络/录放部分已测；AEC参考与音量余量未闭合 |
| M1 | NAS 连续语音 20 轮 | Codex | BACKLOG | M0 完成后再启；当前不链接学习代码 |
| M1.5 | 七命令 pre-LLM router | Codex | BACKLOG | M1；认证/幂等/结果确认契约 |
| M2 | 真实学习闭环 | Codex；可拆 WorkBuddy 辅助 | BACKLOG | M1.5；在线/离线/重启/补传 |
| M2.5 | 离线主动提醒 MVP | Codex | BACKLOG | M2；RTC/TimeAuthority、缓存/内置音频恢复 |
| P0 总验收 | V6 原方案 §10 十一步 | Codex | BACKLOG | 20 轮 + 命令 + 断 NAS 提醒 + 完成 + PWA |
| M3 | Camera/WrongBook 业务 | 待分配 | HOLD | P0 总验收通过 |
| M4 | Learning Memory | 待分配 | HOLD | M3；事实与对话记忆独立 |

WorkBuddy 候选辅助任务：冻结 Board 接口后补硬件矩阵记录模板与重跑脚本；冻结 NAS schema 后补合成集成用例。尚未 READY，不能自行修改 Board/Voice 架构或沿 V5.3 继续接线。实际下发时另给允许路径、Base SHA、独立分支、测试、停止条件，Codex 按不可变提交复核。
