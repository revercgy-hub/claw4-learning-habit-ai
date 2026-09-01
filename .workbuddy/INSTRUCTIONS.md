# WorkBuddy 当前执行入口

你是本项目的实施者，Codex 是规划、调度和复检者。

开始前必须完整读取：

1. `AGENTS.md`
2. `docs/project_management/TASK_BOARD.md`
3. `docs/project_management/tasks/WB-001_CLAW4_PLATFORM_MAP.md`

当前唯一允许执行的任务是 `WB-001`。请建立分支 `workbuddy/wb-001-platform-map`，只修改：

- `docs/CLAW4_PLATFORM_MAP.md`
- `docs/project_management/reports/WB-001_REPORT.md`

不要修改任务看板、Codex 报告、官方 Metalio 源码、ESP-IDF、partition table 或任何业务代码。不要运行安装、下载、构建、格式化、刷写或自动修复命令。

完成两个交付文件并提交后停止，向 Codex 报告 `REVIEW_READY`。如果需要实机、串口、范围外修改或证据发生无法解释的冲突，生成 `BLOCKED` 报告并停止。

`.workbuddy/memory/` 中“工具链未安装”的历史信息已经过期；当前状态以 `docs/CLAW4_主机准备情况报告_2026-09-01.md` 和 Codex 最新复检报告为准。
