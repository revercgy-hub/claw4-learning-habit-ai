# Claw4 学习习惯培育 AI

本仓库是 Claw4 学习习惯养成终端的项目总控仓库。Metalio 官方源码和 ESP-IDF 工具链保留在本地独立目录中，不作为本仓库源码提交。

## 当前协作方式

- Codex：负责总体规划、任务拆分、调度、风险门禁、复检和验收。
- WorkBuddy：只执行 Codex 发布为 `READY` 的单个任务，并提交实现证据与工作报告。
- 用户：确认产品方向、硬件操作、高风险变更和阶段放行。

## 项目入口

- [项目协作总则](AGENTS.md)
- [协作与验收流程](docs/project_management/WORKFLOW.md)
- [唯一任务看板](docs/project_management/TASK_BOARD.md)
- [当前 WorkBuddy 指令](.workbuddy/INSTRUCTIONS.md)
- [Codex 初始复检报告](docs/project_management/reports/CODEX_INITIAL_REVIEW_2026-09-01.md)
- [产品与技术总规划](项目总规划/AGENTS.md)

## 当前结论

主机工具链和官方固件基线编译已通过；当前未检测到串口设备，因此真机 Bring-up 仍被硬件连接门禁阻塞。WorkBuddy 当前唯一可领取任务为 `WB-001`：补齐 Claw4 平台实现映射文档，不修改官方源码或业务代码。
