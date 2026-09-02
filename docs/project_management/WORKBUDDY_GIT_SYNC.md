# WorkBuddy：WB-STREAM-002 Git 同步与连续交付指令

## 1. 当前唯一工作流

- 远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 工作流：`WB-STREAM-002`
- 分支：`workbuddy/domain-offline-stream`
- 任务包：`docs/project_management/tasks/WB-STREAM-002_DOMAIN_OFFLINE.md`
- 顺序：CP0 主机 C++ 运行门槛 → CP1 领域 reducer → CP2 transactional outbox

旧分支 `workbuddy/mvp-core-stream` 已冻结供审计；不得继续在旧分支追加提交。

## 2. 建议使用独立 worktree

先在现有仓库执行只读检查：

```powershell
git status --short --branch
git remote -v
git fetch --prune origin
git ls-remote --heads origin main workbuddy/domain-offline-stream codex/wb-stream-001-review-fixes
git worktree list
```

如果本地尚无新流分支和 worktree：

```powershell
git worktree add --track -b workbuddy/domain-offline-stream E:\workbuddy\claw4-domain-offline-stream origin/workbuddy/domain-offline-stream
Set-Location E:\workbuddy\claw4-domain-offline-stream
```

若本地分支已存在，使用 `git worktree list` 找到它；不得用 reset、删除分支或覆盖另一个 worktree。进入正确目录后：

```powershell
git pull --ff-only origin workbuddy/domain-offline-stream
git status --short --branch
git rev-parse HEAD
git rev-parse origin/workbuddy/domain-offline-stream
git rev-parse origin/main
git merge-base --is-ancestor f021233851a386977aba72b8d15bd8cdd4c0c33f HEAD
```

首次开始的继续条件：

- origin URL 完全匹配上方私有仓库；
- 当前分支为 `workbuddy/domain-offline-stream`；
- 本地 HEAD = 远端工作流分支 = 当时的 `origin/main`；
- 历史包含 Codex 修复 `f021233851a386977aba72b8d15bd8cdd4c0c33f`；
- 工作区干净，且当前目录不是 `vendor/MetalioClaw4`。

任一条件不满足即停止，返回原始命令和输出；禁止 force、reset、rebase、删分支或覆盖文件。

## 3. 必读顺序

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/CONTINUOUS_DEVELOPMENT.md`
4. `docs/project_management/TASK_BOARD.md`
5. `docs/project_management/reports/CODEX_REVIEW_WB-STREAM-001_2026-09-02.md`
6. `docs/ARCHITECTURE.md`
7. `docs/project_management/tasks/WB-STREAM-002_DOMAIN_OFFLINE.md`
8. 本文件

## 4. 每个 checkpoint 的固定循环

```powershell
git status --short --branch
git diff --check
git diff --name-status
# 运行任务包中该 checkpoint 的全部验证
# 更新 WB-STREAM-002_REPORT.md
git diff --check
git diff --name-status
```

只暂存当前 checkpoint 允许路径。提交前必须运行：

```powershell
git diff --cached --check
git diff --cached --name-status
```

### CP0

```powershell
git add -- tools/dev/verify-host-cpp-tests.ps1 firmware/tests/host docs/project_management/reports/WB-STREAM-002_REPORT.md
# 仅任务包允许且确有必要时，另加 .gitignore
git commit -m "test(WB-STREAM-002): add native C++ test gate"
git push -u origin HEAD:workbuddy/domain-offline-stream
```

### CP1

```powershell
git add -- firmware/main/learning_domain firmware/tests/unit/domain tools/dev/verify-host-cpp-tests.ps1 docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "feat(WB-STREAM-002): implement domain reducer"
git push origin HEAD:workbuddy/domain-offline-stream
```

### CP2

```powershell
git add -- firmware/main/sync firmware/tests/unit/sync firmware/tests/fakes tools/dev/verify-host-cpp-tests.ps1 docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "feat(WB-STREAM-002): add transactional outbox core"
git push origin HEAD:workbuddy/domain-offline-stream
```

路径不存在时不要把不存在的参数传给 `git add`；先用 `git status --short` 列出实际新文件，再使用同一允许范围内的精确路径。

## 5. 每次 push 后验证

```powershell
$workbuddyCheckpoint = git rev-parse HEAD
$workbuddyRemote = git ls-remote --heads origin workbuddy/domain-offline-stream
Write-Output $workbuddyCheckpoint
Write-Output $workbuddyRemote
git status --short --branch
```

本地提交必须等于远端提交，工作区必须干净。报告与实现同属一个提交时写“本 checkpoint 提交自身”，精确 hash 使用 push 回执并在下一 checkpoint 回填。push 被拒绝或远端出现未知提交时立即停止，禁止任何历史改写。

## 6. 最终回执格式

```text
工作流：WB-STREAM-002
状态：STREAM_CHECKPOINT_READY / BLOCKED
分支：workbuddy/domain-offline-stream
起始基线：<开始时 HEAD>
CP0_CHECKPOINT：<hash；compiler/version；compile/link/run 结果>
CP1_CHECKPOINT：<hash；domain cases/assertions/pass/fail>
CP2_CHECKPOINT：<hash；sync cases/assertions/pass/fail；5 轮稳定性>
实际修改文件：<按 checkpoint 分组>
范围偏差：无/有（逐项列出）
硬件/串口/Flash/分区操作：必须为 0
真实凭据/儿童数据/业务外部服务：必须为 0
工具链安装：<无；或 LLVM 包 ID/版本/来源/路径，未提交>
HARDWARE_VERIFY_REQUIRED：真实 NVS/掉电/真机并发仍未验证
建议 Codex 复检重点：<清单>
```

CP2 完成后停止扩项并通知 Codex。不得自行合并 `main`，不得开始 UI、NVS/网络适配、官方固件集成或任何硬件任务。
