# WorkBuddy Git 同步与复检交付指令

## 适用范围

本指令用于 WorkBuddy 在每个任务开始前同步 Codex 已发布的任务分支，并在实施后把提交安全推送到项目私有远端，供 Codex 复检。

- 项目远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 当前任务：无
- 上一已验收任务：`WB-HW-001` @ `9867a56121df4cdcdc85f66f4617f3ab8e0b7f81`
- 当前状态：`HOLD`，等待用户硬件证据与重启确认
- 当前允许修改：
  - 无

任务看板没有 `READY` 项时，WorkBuddy 必须停止，不得继续运行下方上一任务的命令，也不得自行创建分支或开始下一阶段。

任何命令结果与本指令不一致时立即停止并报告，不得用 force push、hard reset、覆盖文件或改写远端来“修复”。

## 一、已有本地项目：开始前同步

在项目根目录逐条执行：

```powershell
git status --short --branch
git remote -v
```

继续条件：

- 工作区没有未提交或未跟踪的任务文件；
- `origin` 的 fetch/push URL 都是本指令指定的项目远端；
- 当前目录是项目根仓库，不是 `vendor/MetalioClaw4` 子仓库。

若 `origin` 不存在，可以执行一次：

```powershell
git remote add origin https://github.com/revercgy-hub/claw4-learning-habit-ai.git
```

若 `origin` 已存在但 URL 不一致，停止并把 `git remote -v` 输出交给 Codex；不得自行执行 `git remote set-url`。

确认后执行：

```powershell
git fetch --prune origin
git ls-remote --heads origin main workbuddy/wb-hw-001-readonly-intake
git switch workbuddy/wb-hw-001-readonly-intake
git pull --ff-only origin workbuddy/wb-hw-001-readonly-intake
git status --short --branch
git rev-parse HEAD
```

如果本地尚无任务分支，用下面这一条替代 `git switch` 和 `git pull`：

```powershell
git switch --track -c workbuddy/wb-hw-001-readonly-intake origin/workbuddy/wb-hw-001-readonly-intake
```

开始实施前，`git status --short --branch` 应显示当前分支跟踪 `origin/workbuddy/wb-hw-001-readonly-intake`，且没有待提交文件。

## 二、全新工作目录：首次取得项目

只允许在新的空目录中执行；不得克隆覆盖已有项目目录：

```powershell
git clone https://github.com/revercgy-hub/claw4-learning-habit-ai.git
Set-Location -LiteralPath .\claw4-learning-habit-ai
git fetch --prune origin
git switch --track -c workbuddy/wb-hw-001-readonly-intake origin/workbuddy/wb-hw-001-readonly-intake
git status --short --branch
```

然后完整读取 `AGENTS.md`、任务看板、当前任务包和最新 Codex 复检报告，再开始修改。

## 三、当前任务状态

`WB-HW-001` 已完成并等待 Codex 结果同步；当前没有新的实施任务。

已确认打开 COM3 即使设置 `DtrEnable=false`、`RtsEnable=false` 仍会触发 `CHIP_USB_UART_RESET`。后续任何 COM3 打开操作必须由新的任务包明确授权，并在用户确认可重启设备后执行。Flash、分区、Bootloader、OTA、JTAG、AT 和串口写入继续禁止。

## 四、提交前校验

在任务分支执行：

```powershell
git status --short
git diff --check
git diff --name-only
git diff --name-only origin/main...HEAD
git diff -- docs/BOARD_REVISION.md docs/DEVICE_LOG_REFERENCE.md docs/project_management/reports/WB-HW-001_REPORT.md
```

必须满足：

- 待提交路径只有三个允许文件；
- 所有结论区分 `HOST_ENUM_CONFIRMED`、`DEVICE_LOG_CONFIRMED`、`USER_EVIDENCE_REQUIRED`、`UNKNOWN`；
- 原始日志未进入 Git，只记录仓库外路径、大小、SHA-256 和必要脱敏摘录；
- 没有 `vendor/`、`toolchains/`、构建产物、日志、凭据或密钥；
- 没有把 USB 描述符或源码推断写成实际功能已通过；
- `git diff --check` 无输出。

## 五、提交、推送和远端一致性校验

```powershell
git add -- docs/BOARD_REVISION.md docs/DEVICE_LOG_REFERENCE.md docs/project_management/reports/WB-HW-001_REPORT.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs(WB-HW-001): record read-only device intake"
git push -u origin HEAD:workbuddy/wb-hw-001-readonly-intake
$workbuddyLocalCommit = git rev-parse HEAD
$workbuddyRemoteRef = git ls-remote --heads origin workbuddy/wb-hw-001-readonly-intake
Write-Output $workbuddyLocalCommit
Write-Output $workbuddyRemoteRef
git status --short --branch
```

本地提交号必须与 `git ls-remote` 返回的远端提交号一致。若 push 被拒绝，停止并提交错误原文；不得使用 `--force`、`--force-with-lease`、rebase、reset 或删除远端分支来绕过拒绝。

## 六、交给 Codex 的复检回执

推送成功后，向 Codex 返回以下信息，并停止工作：

```text
任务 ID：WB-HW-001
状态声明：REVIEW_READY
分支：workbuddy/wb-hw-001-readonly-intake
本地 HEAD：<git rev-parse HEAD>
远端 HEAD：<git ls-remote 返回值>
实际修改文件：<git diff --name-only origin/main...HEAD>
提交前校验：git diff --check = PASS
只读边界自检：无刷写/擦除/Flash 读取/串口写入/AT 命令
端口与日志结论：逐项状态和证据
验证命令与关键结果：<摘要>
范围偏差：无/有（若有必须说明并停止）
未解决问题与 HARDWARE_VERIFY_REQUIRED：<清单>
建议 Codex 复检重点：<清单>
```

WorkBuddy 无权把任务标记为 `ACCEPTED`，不得合并到 `main`，不得开始完整 Stage 1、`WB-002` 或任何 MVP 工作。

## 七、Codex 复检入口

WorkBuddy 推送后，Codex 使用以下只读步骤取得证据：

```powershell
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-hw-001-readonly-intake
git diff --check origin/main...origin/workbuddy/wb-hw-001-readonly-intake
git diff --name-status origin/main...origin/workbuddy/wb-hw-001-readonly-intake
git log --oneline --decorate origin/main..origin/workbuddy/wb-hw-001-readonly-intake
```

Codex 根据远端提交而不是 WorkBuddy 的口头描述给出 `ACCEPTED`、`CHANGES_REQUIRED` 或 `BLOCKED`。
