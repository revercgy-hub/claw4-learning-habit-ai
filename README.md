# Claw4 学习习惯培育 AI

2026-09-14最新调度：[A05修订任务书](docs/project_management/tasks/WB-A05-BUILD-001.md) CP0可领取，完整构建等待前序验收与C5输入清单审定。[本轮审查](docs/project_management/reports/CODEX_A05_PLAN_REVIEW_2026-09-14.md)记录19fd979、24/29复跑及5项系统应用控制阻断。WorkBuddy实施；Codex编排、审查和疑难定位，不代替实施。下方首批收口信息保留为历史。

本仓库是 Claw4 学习习惯养成终端的项目总控仓库。Metalio 官方源码和 ESP-IDF 工具链保留在本地独立目录中，不作为本仓库源码提交。

## 当前协作方式

- Codex主agent：负责整体架构、任务调度与审查、整合、风险门禁和疑难问题。
- A01/A02/B01已由Luna/medium子agent实施、Codex审查修订并完成源码/Host验收；按2026-09-14用户决定，下一阶段由WorkBuddy实施、Codex负责代码审查，不自动再开子agent批次。
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

2026-09-14：本批20/20 Host suites及RV32接口语法检查通过，详见[首批收口报告](docs/project_management/reports/CODEX_V53_WAVE1_CLOSEOUT_2026-09-14.md)。当前唯一可领取工作流为[WB-V53-NEXT-001](docs/project_management/tasks/WB-V53-NEXT-001.md)：网络并发→离线终态→仲裁Host→Reminder Host→联合验证，由Codex按提交审查。没有执行本批完整IDF构建或真机测试；历史L1/L3设备证据与后续硬件门禁以看板为准。
