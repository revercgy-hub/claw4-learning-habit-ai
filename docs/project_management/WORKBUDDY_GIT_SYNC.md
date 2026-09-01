# WorkBuddy Git 同步与复检交付指令

## 适用范围

本指令用于 WorkBuddy 同步 Codex 已发布的唯一任务分支，并把实施提交以普通快进推送到项目私有远端，供 Codex 复检。

- 项目远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 当前唯一任务：`WB-HW-002`
- 当前状态：`CHANGES_REQUIRED`
- 任务分支：`workbuddy/wb-hw-002-controlled-boot`
- 任务包：`docs/project_management/tasks/WB-HW-002_CONTROLLED_BOOT_CAPTURE.md`
- 最新复检：`docs/project_management/reports/CODEX_REVIEW_WB-HW-002_2026-09-01.md`
- 当前允许修改：
  - `docs/BOARD_REVISION.md`
  - `docs/DEVICE_LOG_REFERENCE.md`
  - `docs/PHYSICAL_INSPECTION.md`
  - `docs/project_management/reports/WB-HW-002_REPORT.md`

任何命令结果与本指令不一致时立即停止并报告。不得用 force push、hard reset、rebase、删除分支、覆盖文件或改写远端来“修复”。

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
git ls-remote --heads origin main workbuddy/wb-hw-002-controlled-boot
git switch workbuddy/wb-hw-002-controlled-boot
git pull --ff-only origin workbuddy/wb-hw-002-controlled-boot
git status --short --branch
git rev-parse HEAD
git rev-parse origin/workbuddy/wb-hw-002-controlled-boot
```

如果本地尚无任务分支，用下面这一条替代 `git switch` 和 `git pull`：

```powershell
git switch --track -c workbuddy/wb-hw-002-controlled-boot origin/workbuddy/wb-hw-002-controlled-boot
```

开始实施前，本地 HEAD 必须等于 `origin/workbuddy/wb-hw-002-controlled-boot`，工作区必须干净。若同名本地分支存在但不能普通快进，停止并报告，不得 reset/rebase。

## 二、全新工作目录：首次取得项目

只允许在新的空目录中执行；不得克隆覆盖已有项目目录：

```powershell
git clone https://github.com/revercgy-hub/claw4-learning-habit-ai.git
Set-Location -LiteralPath .\claw4-learning-habit-ai
git fetch --prune origin
git switch --track -c workbuddy/wb-hw-002-controlled-boot origin/workbuddy/wb-hw-002-controlled-boot
git status --short --branch
git rev-parse HEAD
```

然后完整读取 `AGENTS.md`、任务看板、当前任务包、`CODEX_REVIEW_WB-HW-002_2026-09-01.md` 和现有任务报告。本轮只能处理复检项，不得重新执行硬件采集。

## 三、本轮修订指令

本轮不再授权任何设备操作。WorkBuddy 只处理最新 Codex 报告中的四项：

1. `CR-WBHW002-01`：把扬声器、麦克风、SIM/SD、Pogo/磁吸、单摄和材质判断全部降级为照片能直接支持的形态描述，并在四份交付物中保持一致。
2. `CR-WBHW002-02`：把 WiFi/4G、固件刷新、UUID 生成方式和“稳定运行”结论降级到原始日志能直接支持的范围。
3. `CR-WBHW002-03`：清除报告占位符和“预期/待校验”；填写初次提交 `63281b23c85433e184660517e8ecaeddabff94c7` 与实测 Git 结果；补存并索引实际执行的采集脚本/命令，或如实声明无法恢复；解释 PnP 首次过滤 `NOT FOUND` 与后续精确查询成功的差异。
4. `CR-WBHW002-04`：只修改原四个交付文件并普通快进推送。

禁止再次打开 COM3，禁止打开其他串口、重新采集、发送数据、操作 JTAG/AT/Flash、运行 esptool、刷写或修改固件。不得为了补证据重演硬件过程，也不得事后编造无法恢复的命令原文。

## 四、提交前校验

在任务分支执行：

```powershell
git status --short
git diff --check
git diff --name-only
git diff --name-only origin/main...HEAD
git diff -- docs/BOARD_REVISION.md docs/DEVICE_LOG_REFERENCE.md docs/PHYSICAL_INSPECTION.md docs/project_management/reports/WB-HW-002_REPORT.md
```

必须满足：

- 待提交路径只有四个允许文件；
- 四个 CR 项逐项回应，并在报告中给出修改位置；
- `docs/PHYSICAL_INSPECTION.md` 不再包含接口功能、材料或结构类型猜测；
- 报告明确记录初次交付提交、实际远端校验结果、`Open()` 次数、写入字节数、采集时长和参数；
- 实际执行过的采集脚本/完整命令已作为仓库外证据索引；若无法恢复则明确披露证据缺口，禁止伪造；
- PnP 初次查询 `NOT FOUND` 与后续精确查询成功已解释；
- WiFi/4G、镜像差异、UUID 和稳定性结论没有超出日志原文；
- 原始日志/照片未进入 Git，只记录仓库外路径、大小、SHA-256 和必要脱敏摘录；
- 没有 `vendor/`、`toolchains/`、构建产物、日志、凭据或密钥；
- 未把照片外观、USB 描述符或源码推断写成实际功能已通过；
- `git diff --check` 无输出。

## 五、提交、推送和远端一致性校验

```powershell
git add -- docs/BOARD_REVISION.md docs/DEVICE_LOG_REFERENCE.md docs/PHYSICAL_INSPECTION.md docs/project_management/reports/WB-HW-002_REPORT.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs(WB-HW-002): correct evidence boundaries"
git push -u origin HEAD:workbuddy/wb-hw-002-controlled-boot
$workbuddyLocalCommit = git rev-parse HEAD
$workbuddyRemoteRef = git ls-remote --heads origin workbuddy/wb-hw-002-controlled-boot
Write-Output $workbuddyLocalCommit
Write-Output $workbuddyRemoteRef
git status --short --branch
```

本地提交号必须与 `git ls-remote` 返回的远端提交号一致。若 push 被拒绝，停止并提交错误原文；不得使用 `--force`、`--force-with-lease`、rebase、reset 或删除远端分支来绕过拒绝。

## 六、交给 Codex 的复检回执

推送成功后，向 Codex 返回以下信息，并停止工作：

```text
任务 ID：WB-HW-002
状态声明：REVIEW_READY / BLOCKED
分支：workbuddy/wb-hw-002-controlled-boot
开始提交：<开始前 git rev-parse HEAD>
本地 HEAD：<提交后 git rev-parse HEAD>
远端 HEAD：<git ls-remote 返回值>
实际修改文件：<git diff --name-only origin/main...HEAD>
提交前校验：git diff --check = PASS/FAIL
COM3 PnP 身份：<实例 ID、VID/PID、描述>
CR-WBHW002-01：<修改位置与结果>
CR-WBHW002-02：<修改位置与结果>
CR-WBHW002-03：<修改位置与结果；脚本/命令证据路径和哈希，或无法恢复声明>
CR-WBHW002-04：<范围与 Git 校验>
本轮硬件操作：0（不得重新打开 COM3）
原采集 Open() 声明：1
原采集串口写入声明：0 B
原采集参数与时长：<波特率、8N1、DTR/RTS、起止时间、秒数、字节数>
复位与启动原始证据：<只写日志直接支持的事实>
仓库外证据：<路径、文件大小、SHA-256>
敏感信息检查与脱敏：<结果>
照片证据结论：<纯形态事实与仍未知项>
范围偏差：无/有（若有必须说明并停止）
未解决问题与 HARDWARE_VERIFY_REQUIRED：<清单>
建议 Codex 复检重点：<清单>
```

WorkBuddy 无权把任务标记为 `ACCEPTED`，不得修改任务看板、合并到 `main` 或开始下一任务。

## 七、Codex 复检入口

WorkBuddy 推送后，Codex 使用以下只读步骤取得证据：

```powershell
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-hw-002-controlled-boot
git diff --check origin/main...origin/workbuddy/wb-hw-002-controlled-boot
git diff --name-status origin/main...origin/workbuddy/wb-hw-002-controlled-boot
git log --oneline --decorate origin/main..origin/workbuddy/wb-hw-002-controlled-boot
```

Codex 还会独立复验仓库外证据文件大小、SHA-256、日志关键行、一次 `Open()`/零写入记录和照片索引，再给出 `ACCEPTED`、`CHANGES_REQUIRED` 或 `BLOCKED`。
