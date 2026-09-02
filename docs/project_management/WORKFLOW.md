# Codex / WorkBuddy 协作与验收流程

## 目标

把“Codex 负责规划、异步复检与后续修复；WorkBuddy 负责连续实施”落实为仓库内可追踪、可复现、可回滚的工作流。

## 职责边界

| 环节 | Codex | WorkBuddy | 用户 |
| --- | --- | --- | --- |
| 阶段规划 | 负责 | 提供实施反馈 | 确认方向 |
| 任务拆分与依赖 | 负责 | 不自行扩项 | 可调整优先级 |
| 具体实现 | 在 WorkBuddy checkpoint 后直接修复缺陷并补测 | 负责连续主实现 | 提供必要硬件操作 |
| 测试执行 | 指定范围、复跑关键项 | 首次执行并记录 | 配合高风险/物理操作 |
| 验收结论 | 负责 | 不自验收 | 最终争议裁决 |
| Git 调度 | 定义分支与合入门槛 | 任务分支提交 | 提供远端与权限 |

## 调度循环

1. Codex 读取最新证据，更新 `TASK_BOARD.md` 并发布一个活动连续工作流。
2. 任务包预先列出有序 checkpoint、允许路径、验证与停止条件。
3. WorkBuddy 在工作流分支逐 checkpoint 实施；每项验证、报告、提交和 push 后立即继续下一项。
4. Codex 按远端不可变 checkpoint 异步检查范围、diff、测试、风险标签和提交。
5. 普通缺陷由 Codex 在独立 `codex/` 修复分支直接修改并补测；P0 门禁问题暂停受影响的后续工作。
6. 工作流收口后，Codex 给出 `ACCEPTED`、`CHANGES_REQUIRED` 或 `BLOCKED` 并决定是否合入 `main`。

详细规则见 [`CONTINUOUS_DEVELOPMENT.md`](CONTINUOUS_DEVELOPMENT.md)。

## 阶段门禁

| 门禁 | 通过条件 | 当前状态 |
| --- | --- | --- |
| G0 主机基线 | IDF v5.5.4、P4 工具链、Python 依赖、官方 baseline 构建产物存在 | `PASSED` |
| G1 平台证据 | 仓库审计和平台实现映射被 Codex 接受 | `PASSED`：WB-001 @ `45b2c73` 已验收 |
| G2 真机 Stage 1 | B001/B002/B003/B004/B005/B009/B013 有实机证据且复检通过 | `INCOMPLETE`：WB-HW-001/WB-HW-002 已验收；完整屏幕、触摸、音频、存储、电源等 Stage 1 仍未通过，未授权 Flash 访问 |
| G3 MVP 开发准入 | Bring-up 准入项满足并给出 GO/GO WITH CONDITIONS | `CONDITIONAL`：用户已授权隔离分支上的纯主机接口、Mock Backend 与自动化测试连续开发；真机耦合、发布固件和硬件声明仍 `HOLD` |
| G4 MVP 闭环 | 设备到后端到家长端的在线与离线闭环均通过 | `HOLD` |

## Git 规则

- 项目根仓库管理规划、脚本、业务代码和可审查报告。
- 项目协作远端固定为 `https://github.com/revercgy-hub/claw4-learning-habit-ai.git`；WorkBuddy 开始和交付任务时必须执行 [`WORKBUDDY_GIT_SYNC.md`](WORKBUDDY_GIT_SYNC.md)。
- `vendor/` 与 `toolchains/` 是本地依赖，保留各自历史，不进入根仓库。
- WorkBuddy 不直接在 `main` 上实施；分支名为 `workbuddy/<task-id>-<short-name>`。
- WorkBuddy checkpoint 可连续推进；Codex 工作流复检通过后才允许合入主线。
- Codex 后续修复使用 `codex/<stream-id>-review-fixes`，不得与 WorkBuddy 在同一分支并发写入。
- WorkBuddy 只允许普通 push 当前任务分支，禁止 force push、改写 `main`、删除远端分支或修改远端 URL。

## 阻塞与停止规则

出现以下任一情况，WorkBuddy 应停止并报告，不得猜测：

- 任务包要求的文件或工具不存在；
- 需要修改任务包允许路径之外的文件；
- 需要真机、串口、账号、密钥或外部服务但当前不可用；
- 官方文档、源码、sdkconfig 或实机证据互相冲突；
- 操作可能刷写、擦除、覆盖固件或改变安全/分区配置；
- 测试失败且继续操作可能破坏基线。
