# WorkBuddy 连续工作流 Git 同步与交付指令

## 当前工作流

- 远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 工作流：`WB-STREAM-001`
- 分支：`workbuddy/mvp-core-stream`
- 任务包：`docs/project_management/tasks/WB-STREAM-001_MVP_CORE.md`
- 执行顺序：CP0 → CP1 → CP2；checkpoint 之间不等待 Codex

## 一、同步工作流分支

```powershell
git status --short --branch
git remote -v
git fetch --prune origin
git ls-remote --heads origin main workbuddy/mvp-core-stream
git switch workbuddy/mvp-core-stream
git pull --ff-only origin workbuddy/mvp-core-stream
git status --short --branch
git rev-parse HEAD
git rev-parse origin/workbuddy/mvp-core-stream
```

若本地没有该分支：

```powershell
git switch --track -c workbuddy/mvp-core-stream origin/workbuddy/mvp-core-stream
```

继续条件：

- 远端 URL 完全正确；
- 本地 HEAD 等于远端工作流分支；
- 工作区干净；
- 当前目录不是 `vendor/MetalioClaw4`；
- 分支历史包含原 WB-002 checkpoint `465223e370e034bbdfd81e340476c1f3e8d12957` 和最新项目管理基线。

任一条件不满足立即停止；不得 force、reset、rebase、删除分支或覆盖文件。

## 二、必须读取

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/CONTINUOUS_DEVELOPMENT.md`
4. `docs/project_management/TASK_BOARD.md`
5. `docs/project_management/tasks/WB-STREAM-001_MVP_CORE.md`
6. `docs/project_management/reports/CODEX_REVIEW_WB-002_ROUND2_2026-09-02.md`
7. `docs/ARCHITECTURE.md`

## 三、连续 checkpoint 循环

对 CP0、CP1、CP2 依次执行基础检查：

```powershell
git status --short --branch
# 只修改当前 checkpoint 允许路径
# 运行该 checkpoint 的全部验证
git diff --check
git diff --name-status
```

各 checkpoint 使用下列精确暂存与提交命令。

### CP0

```powershell
git add -- docs/ARCHITECTURE.md docs/project_management/reports/WB-002_REPORT.md docs/project_management/reports/WB-STREAM-001_REPORT.md
git diff --cached --check
git diff --cached --name-status
git commit -m "docs(WB-002): close replay and claim contracts"
git push -u origin HEAD:workbuddy/mvp-core-stream
```

### CP1

```powershell
git add -- firmware/main firmware/tests/contracts tools/dev/verify-interface-contracts.ps1 docs/project_management/reports/WB-STREAM-001_REPORT.md
git diff --cached --check
git diff --cached --name-status
git commit -m "feat(WB-STREAM-001): add MVP interface contracts"
git push -u origin HEAD:workbuddy/mvp-core-stream
```

### CP2

```powershell
git add -- backend docs/project_management/reports/WB-STREAM-001_REPORT.md
git diff --cached --check
git diff --cached --name-status
git commit -m "feat(WB-STREAM-001): add contract mock backend"
git push -u origin HEAD:workbuddy/mvp-core-stream
```

每次 push 后验证：

```powershell
$workbuddyCheckpoint = git rev-parse HEAD
$workbuddyRemote = git ls-remote --heads origin workbuddy/mvp-core-stream
Write-Output $workbuddyCheckpoint
Write-Output $workbuddyRemote
git status --short --branch
```

本地提交必须等于远端提交且工作区干净。当前报告与代码同属一个提交时，不得制造自引用 hash 占位符：报告写“本 checkpoint 提交自身”，精确 hash 使用 push 回执；在下一 checkpoint 更新报告时回填上一 checkpoint 的精确 hash。随后直接继续下一 checkpoint。

push 被拒绝或远端出现未知提交时停止并返回原始错误；禁止 force、rebase、reset 或删除远端分支。

## 四、CP0 特别验证

```powershell
rg -n "duplicate|event_id|sequence|last_acked|响应丢失|幂等|回退|冲突" docs/ARCHITECTURE.md
rg -n "challenge|nonce|过期|单次|重放|签名" docs/ARCHITECTURE.md
rg -n "claim|parent_id|child_id|认证家长|授权范围|通用错误" docs/ARCHITECTURE.md
rg -n "快照损坏|auto_saved|aborted|显式|task.completed" docs/ARCHITECTURE.md
```

还必须重新验证所有 JSON 代码块、本地 Markdown 链接、证据标签和章节，并扫描旧冲突语义。

## 五、最终回执

```text
工作流：WB-STREAM-001
状态：STREAM_CHECKPOINT_READY / BLOCKED
分支：workbuddy/mvp-core-stream
起始基线：<开始时 HEAD>
CP0_CHECKPOINT：<commit + 验证摘要>
CP1_CHECKPOINT：<commit + 交叉编译/依赖扫描摘要>
CP2_CHECKPOINT：<commit + pytest 数量和结果>
实际修改文件：<按 checkpoint 分组>
范围偏差：无/有
硬件/串口/Flash 操作：必须为 0
真实凭据/儿童数据/外部服务：必须为 0
建议 Codex 直接修复重点：<清单>
```

CP2 完成后停止扩项并通知 Codex。WorkBuddy 不得自行合并 `main` 或开始领域实现、UI、真机集成、Voice、Camera、AI、OTA 等后续任务。
