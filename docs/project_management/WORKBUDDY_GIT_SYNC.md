# WorkBuddy：WB-STREAM-002 Git 同步与连续交付指令

## 1. 当前唯一工作流

- 远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 工作流：`WB-STREAM-002`
- 分支：`workbuddy/domain-offline-stream`
- 任务包：`docs/project_management/tasks/WB-STREAM-002_DOMAIN_OFFLINE.md`
- 顺序：CP0～CP8 连续执行；CP8 后才停止并交由 Codex 最终验收

旧分支 `workbuddy/mvp-core-stream` 已冻结供审计；不得继续在旧分支追加提交。

Codex 已创建本地 worktree `E:\workbuddy\claw4-domain-offline-stream`。优先直接使用该目录；截至预检时没有任何 WorkBuddy checkpoint，必须从 CP0 开始。

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

若已知外部机制再次删除本地 `refs/heads/workbuddy/*`，但 worktree 文件与 HEAD 未受损：先用 `git status`、`git rev-parse HEAD` 和 `git ls-remote` 保存证据；可以保持 detached HEAD 连续提交，并始终使用显式 `git push origin HEAD:refs/heads/workbuddy/domain-offline-stream`。不得为恢复本地分支而 reset/rebase/force，也不得把 ref 异常当作跳过 checkpoint 的理由。

## 3. 必读顺序

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/CONTINUOUS_DEVELOPMENT.md`
4. `docs/project_management/TASK_BOARD.md`
5. `docs/project_management/reports/CODEX_REVIEW_WB-STREAM-001_2026-09-02.md`
6. `docs/project_management/reports/CODEX_PREFLIGHT_WB-STREAM-002_2026-09-02.md`
7. `docs/ARCHITECTURE.md`
8. `docs/project_management/tasks/WB-STREAM-002_DOMAIN_OFFLINE.md`
9. 本文件

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

### CP3

```powershell
git add -- firmware/main/application firmware/main/sync firmware/tests/unit/application firmware/tests/unit/sync firmware/tests/fakes tools/dev/verify-host-cpp-tests.ps1 docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "feat(WB-STREAM-002): integrate host application coordinator"
git push origin HEAD:workbuddy/domain-offline-stream
```

### CP4

```powershell
git add -- firmware/main/ui firmware/tests/unit/ui firmware/tests/fakes tools/dev/verify-host-cpp-tests.ps1 docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "feat(WB-STREAM-002): add host UI presenters"
git push origin HEAD:workbuddy/domain-offline-stream
```

### CP5

```powershell
git add -- backend deploy docker-compose.yml .env.example docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "feat(WB-STREAM-002): add persistent family backend"
git push origin HEAD:workbuddy/domain-offline-stream
```

### CP6

```powershell
git add -- frontend docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "feat(WB-STREAM-002): add parent PWA"
git push origin HEAD:workbuddy/domain-offline-stream
```

### CP7

```powershell
git add -- backend/tests/e2e backend/tests/fixtures frontend firmware/tests tools/dev/run-host-mvp-e2e.ps1 docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "test(WB-STREAM-002): verify host MVP loop"
git push origin HEAD:workbuddy/domain-offline-stream
```

### CP8

```powershell
git add -- firmware/main/learning_domain firmware/main/sync firmware/main/application firmware/main/ui firmware/tests backend frontend deploy docker-compose.yml .env.example tools/dev docs/HOST_MVP_ACCEPTANCE.md docs/project_management/reports/WB-STREAM-002_REPORT.md
git commit -m "test(WB-STREAM-002): stabilize host MVP evidence"
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
状态：STREAM_REVIEW_READY / BLOCKED
分支：workbuddy/domain-offline-stream
起始基线：<开始时 HEAD>
CP0_CHECKPOINT：<hash；compiler/version；compile/link/run 结果>
CP1_CHECKPOINT：<hash；domain cases/assertions/pass/fail>
CP2_CHECKPOINT：<hash；sync cases/assertions/pass/fail；5 轮稳定性>
CP3_CHECKPOINT：<hash；coordinator/cache/retry case 结果>
CP4_CHECKPOINT：<hash；UI presenter case 结果>
CP5_CHECKPOINT：<hash；backend tests/pip check/迁移与 Compose 静态验证>
CP6_CHECKPOINT：<hash；typecheck/lint/test/build/audit>
CP7_CHECKPOINT：<hash；host E2E 闭环结果>
CP8_CHECKPOINT：<hash；全部稳定性轮次与 HOST_MVP_ACCEPTANCE>
实际修改文件：<按 checkpoint 分组>
范围偏差：无/有（逐项列出）
硬件/串口/Flash/分区操作：必须为 0
真实凭据/儿童数据/业务外部服务：必须为 0
工具链安装：<无；或 LLVM 包 ID/版本/来源/路径，未提交>
HARDWARE_VERIFY_REQUIRED：真实 NVS/掉电、TLS/设备网络、LVGL、真机并发与完整真机闭环仍未验证
建议 Codex 复检重点：<清单>
```

CP0～CP8 之间不等待 Codex；CP8 完成后停止扩项并通知 Codex。不得自行合并 `main`，不得开始 LVGL、真实 NVS/设备网络适配、官方固件集成或任何硬件任务。
