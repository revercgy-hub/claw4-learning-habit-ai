# WorkBuddy Git 同步与复检交付指令

## 适用范围

本指令用于 WorkBuddy 在每个任务开始前同步 Codex 已发布的任务分支，并在实施后把提交安全推送到项目私有远端，供 Codex 复检。

- 项目远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 当前任务：`WB-001`
- 当前任务分支：`workbuddy/wb-001-platform-map`
- 当前状态：`CHANGES_REQUIRED`
- 当前待修订实施提交：`a829765432b69b12ce4bf6ff0e06dc9458372d57`
- 当前允许修改：
  - `docs/CLAW4_PLATFORM_MAP.md`
  - `docs/project_management/reports/WB-001_REPORT.md`

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
git ls-remote --heads origin main workbuddy/wb-001-platform-map
git switch workbuddy/wb-001-platform-map
git pull --ff-only origin workbuddy/wb-001-platform-map
git status --short --branch
git rev-parse HEAD
```

如果本地尚无任务分支，用下面这一条替代 `git switch` 和 `git pull`：

```powershell
git switch --track -c workbuddy/wb-001-platform-map origin/workbuddy/wb-001-platform-map
```

开始实施前，`git status --short --branch` 应显示当前分支跟踪 `origin/workbuddy/wb-001-platform-map`，且没有待提交文件。

## 二、全新工作目录：首次取得项目

只允许在新的空目录中执行；不得克隆覆盖已有项目目录：

```powershell
git clone https://github.com/revercgy-hub/claw4-learning-habit-ai.git
Set-Location -LiteralPath .\claw4-learning-habit-ai
git fetch --prune origin
git switch --track -c workbuddy/wb-001-platform-map origin/workbuddy/wb-001-platform-map
git status --short --branch
```

然后完整读取 `AGENTS.md`、任务看板、当前任务包和最新 Codex 复检报告，再开始修改。

## 三、当前 WB-001 修订要求

CR-WB001-01～04 的技术内容已经通过二次复检。当前只处理 `CODEX_REVIEW_WB-001_ROUND2_2026-09-01.md` 中的证据行号修订：

1. 把 `esp_lvgl_port_disp.c:256-262` 修正为实际覆盖区间 `:317-325`，其中 ESP32-P4 的赋值与两个 frame buffer 获取位于 `:323-324`。
2. 把 FL7707N 的证据改为 `metalio-claw-4.cc:356`（RGB888）和 `:392`（`bits_per_pixel = 16`），不得继续用未包含实际赋值的 `:355-376` 作为完整证据。
3. BQ27220 绑定如保留行号，应引用实际调用 `metalio-claw-4.cc:609`，或完整上下文 `:606-610`。

除上述引用修订外，不重写已经通过的技术结论。

不得修改看板、本同步指令、Codex 报告、官方源码、配置、分区表或任何其他文件。

## 四、提交前校验

在任务分支执行：

```powershell
git status --short
git diff --check
git diff --name-only
git diff --name-only origin/main...HEAD
git diff -- docs/CLAW4_PLATFORM_MAP.md docs/project_management/reports/WB-001_REPORT.md
```

必须满足：

- 待提交路径只有两个允许文件；
- CR-WB001-01～04 在平台映射和任务报告中均已反映；
- 没有 `vendor/`、`toolchains/`、构建产物、日志、凭据或密钥；
- 没有把未连接真机的能力写成实机已确认；
- `git diff --check` 无输出。

## 五、提交、推送和远端一致性校验

```powershell
git add -- docs/CLAW4_PLATFORM_MAP.md docs/project_management/reports/WB-001_REPORT.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs(WB-001): correct evidence line references"
git push -u origin HEAD:workbuddy/wb-001-platform-map
$workbuddyLocalCommit = git rev-parse HEAD
$workbuddyRemoteRef = git ls-remote --heads origin workbuddy/wb-001-platform-map
Write-Output $workbuddyLocalCommit
Write-Output $workbuddyRemoteRef
git status --short --branch
```

本地提交号必须与 `git ls-remote` 返回的远端提交号一致。若 push 被拒绝，停止并提交错误原文；不得使用 `--force`、`--force-with-lease`、rebase、reset 或删除远端分支来绕过拒绝。

## 六、交给 Codex 的复检回执

推送成功后，向 Codex 返回以下信息，并停止工作：

```text
任务 ID：WB-001
状态声明：REVIEW_READY
分支：workbuddy/wb-001-platform-map
本地 HEAD：<git rev-parse HEAD>
远端 HEAD：<git ls-remote 返回值>
实际修改文件：<git diff --name-only origin/main...HEAD>
提交前校验：git diff --check = PASS
四项修订自检：CR-WB001-01～04 = PASS/逐项说明
验证命令与关键结果：<摘要>
范围偏差：无/有（若有必须说明并停止）
未解决问题与 HARDWARE_VERIFY_REQUIRED：<清单>
建议 Codex 复检重点：<清单>
```

WorkBuddy 无权把任务标记为 `ACCEPTED`，不得合并到 `main`，不得开始 `WB-002` 或任何 Bring-up/MVP 工作。

## 七、Codex 复检入口

WorkBuddy 推送后，Codex 使用以下只读步骤取得证据：

```powershell
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-001-platform-map
git diff --check origin/main...origin/workbuddy/wb-001-platform-map
git diff --name-status origin/main...origin/workbuddy/wb-001-platform-map
git log --oneline --decorate origin/main..origin/workbuddy/wb-001-platform-map
```

Codex 根据远端提交而不是 WorkBuddy 的口头描述给出 `ACCEPTED`、`CHANGES_REQUIRED` 或 `BLOCKED`。
