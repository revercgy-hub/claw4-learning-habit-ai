# Claw4 学习习惯培育 AI

本仓库是 Claw4 学习习惯养成终端的项目总控仓库。Metalio 官方源码和 ESP-IDF 工具链保留在本地独立目录中，不作为本仓库源码提交。

## 当前协作方式

- Codex主agent：负责整体架构、任务调度与审查、整合、风险门禁和疑难问题。
- 子agent：领取边界明确的工作包，在独立分支实现、验证并交付审查；WorkBuddy继续参与时也遵循同一队列。
- 用户：确认产品方向、硬件操作、高风险变更和阶段放行。

2026-09-13当前入口：[V5.3优化架构](docs/ARCHITECTURE_V5_3.md)、[任务看板](docs/project_management/TASK_BOARD.md)、[子agent工作包](docs/project_management/tasks/CODEX-V53_AGENT_WORK_PACKAGES.md)、[最新进展核验](docs/project_management/reports/CODEX_V53_FACT_SYNC_2026-09-13.md)。设备开发基线为 `workbuddy-app-first-l3-acceptance` @ `7dd6511`，main尚未整合该开发链。

## 项目入口

- [项目协作总则](AGENTS.md)
- [协作与验收流程](docs/project_management/WORKFLOW.md)
- [唯一任务看板](docs/project_management/TASK_BOARD.md)
- [当前 WorkBuddy 指令](.workbuddy/INSTRUCTIONS.md)
- [Codex 初始复检报告](docs/project_management/reports/CODEX_INITIAL_REVIEW_2026-09-01.md)
- [产品与技术总规划](项目总规划/AGENTS.md)

## 当前结论

主机工具链和官方固件基线编译已通过；当前未检测到串口设备，因此真机 Bring-up 仍被硬件连接门禁阻塞。WorkBuddy 当前唯一可领取任务为 `WB-001`：补齐 Claw4 平台实现映射文档，不修改官方源码或业务代码。
