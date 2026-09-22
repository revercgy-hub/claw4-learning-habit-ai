# V6 唯一阶段看板

更新：2026-09-22。负责人：Codex。当前活动：CODEX-V6-FOUNDATION / IN_PROGRESS；没有已下发的 WorkBuddy V6 工作流。V5.3 继续 Voice 补丁与外围扩展 HOLD，历史报告保留，调度状态 SUPERSEDED。

| 顺序 | 工作 | 负责人 | 状态 | 放行与证据 |
| --- | --- | --- | --- | --- |
| 0 | 架构/迁移构建入口/边界检查 | Codex | REVIEW_READY | 代码/文档已交付；78 case PASS，协调器受应用控制阻断，不能标全量 PASS |
| M0-1 | 四方源码冻结 + 环境/恢复清单 | Codex | IN_PROGRESS | SHA/IDF6.1/依赖锁已冻结；32MiB 完整备份和实际布局已核对，恢复写回尚未实测 |
| M0-2 | Claw4 Board Port | Codex | IN_PROGRESS | m0.1 构建通过；屏幕/触摸/音频/C5 诊断候选，Camera/SD/电源键待补 |
| M0-3 | 硬件矩阵验证 | Codex + 用户屏侧反馈 | IN_PROGRESS | 四轮刷机：启动/资源加载通过；显示由用户确认，触摸事件通过；音频效果待确认，详见 V6_M0_DEVICE_REPORT.md |
| M1 | NAS 连续语音 20 轮 | Codex | BACKLOG | M0 真机通过；不链接学习代码 |
| M1.5 | 七命令 pre-LLM router | Codex | BACKLOG | M1；认证/幂等/结果确认契约 |
| M2 | 真实学习闭环 | Codex；可拆 WorkBuddy 辅助 | BACKLOG | M1.5；在线/离线/重启/补传 |
| M2.5 | 离线主动提醒 MVP | Codex | BACKLOG | M2；RTC/TimeAuthority、缓存/内置音频恢复 |
| P0 总验收 | V6 原方案 §10 十一步 | Codex | BACKLOG | 20 轮 + 命令 + 断 NAS 提醒 + 完成 + PWA |
| M3 | Camera/WrongBook 业务 | 待分配 | HOLD | P0 总验收通过 |
| M4 | Learning Memory | 待分配 | HOLD | M3；事实与对话记忆独立 |

WorkBuddy 候选辅助任务：冻结 Board 接口后补硬件矩阵记录模板与重跑脚本；冻结 NAS schema 后补合成集成用例。尚未 READY，不能自行修改 Board/Voice 架构或沿 V5.3 继续接线。实际下发时另给允许路径、Base SHA、独立分支、测试、停止条件，Codex 按不可变提交复核。
