# WorkBuddy Git 同步与复检交付指令

## 适用范围

- 项目远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 当前唯一任务：`WB-002`
- 当前状态：`READY`
- 任务分支：`workbuddy/wb-002-architecture`
- 任务包：`docs/project_management/tasks/WB-002_ARCHITECTURE.md`
- 允许修改：
  - `docs/ARCHITECTURE.md`
  - `docs/project_management/reports/WB-002_REPORT.md`

本任务是开发前架构设计，只写上述两个文档。不得创建或修改业务源码、固件、构建配置、Docker、官方 BSP、分区或测试工程，不得操作硬件。

任何命令结果与本指令不一致时立即停止并报告。禁止 force push、hard reset、rebase、删除分支、覆盖文件或改写远端。

## 一、已有本地项目同步

在项目根目录逐条执行：

```powershell
git status --short --branch
git remote -v
```

继续条件：

- 工作区干净；
- `origin` 的 fetch/push URL 是 `https://github.com/revercgy-hub/claw4-learning-habit-ai.git`；
- 当前目录是项目根仓库，不是 `vendor/MetalioClaw4`。

然后执行：

```powershell
git fetch --prune origin
git ls-remote --heads origin main workbuddy/wb-002-architecture
git switch workbuddy/wb-002-architecture
git pull --ff-only origin workbuddy/wb-002-architecture
git status --short --branch
git rev-parse HEAD
git rev-parse origin/workbuddy/wb-002-architecture
```

若本地尚无该任务分支，用下面这一条替代 `switch` 和 `pull`：

```powershell
git switch --track -c workbuddy/wb-002-architecture origin/workbuddy/wb-002-architecture
```

开始前本地 HEAD 必须等于远端任务分支，且工作区干净。若本地分支不能普通快进，停止并报告，不得 reset/rebase。

## 二、全新工作目录

只允许在新的空目录中执行：

```powershell
git clone https://github.com/revercgy-hub/claw4-learning-habit-ai.git
Set-Location -LiteralPath .\claw4-learning-habit-ai
git fetch --prune origin
git switch --track -c workbuddy/wb-002-architecture origin/workbuddy/wb-002-architecture
git status --short --branch
git rev-parse HEAD
```

然后完整读取根 `AGENTS.md`、`项目总规划/AGENTS.md`、任务看板、当前任务包及任务包列出的全部输入。

## 三、任务边界

严格执行 `WB-002_ARCHITECTURE.md`：

- 建立设备、家庭后端、PWA、AI Gateway 与数据库的架构和数据所有权边界；
- 定义 Task、StudySession、Event、设备/会话状态机、离线队列、幂等、重试和安全隐私契约；
- 结合官方源码只读检查写清设备侧模块依赖；不得修改 `vendor/MetalioClaw4`；
- 所有建议实现标记为 `ARCH_DECISION`，不得冒充现有代码；
- 所有未被真机证据确认的硬件事实保持 `HARDWARE_VERIFY_REQUIRED` 或 `UNKNOWN`；
- 不写任何产品代码，不创建目录骨架，不运行构建，不连接设备，不访问 Flash。

需要选定重大技术栈或持久化方案且现有事实不足时停止，记录候选方案和权衡交给 Codex，不得自行扩大范围。

## 四、提交前校验

```powershell
git status --short
git diff --check
git diff --name-only
git diff -- docs/ARCHITECTURE.md docs/project_management/reports/WB-002_REPORT.md
rg -n "PRODUCT_REQUIRED|SOURCE_CONFIRMED|DEVICE_LOG_CONFIRMED|ARCH_DECISION|HARDWARE_VERIFY_REQUIRED|UNKNOWN" docs/ARCHITECTURE.md
rg -n "Task|StudySession|event_id|sequence|at-least-once|幂等|离线|GPIO|LVGL|HTTPS|WSS|verify=false" docs/ARCHITECTURE.md
```

必须满足：

- 只出现两个允许文件；
- `docs/ARCHITECTURE.md` 覆盖任务包12个章节；
- JSON 示例可解析，本地 Markdown 链接存在；
- 没有真实密钥、token、儿童数据或付费服务配置；
- 没有把建议目录、接口或技术栈写成已实现；
- 没有把源码/配置事实写成实机已验证；
- `git diff --check` 无输出。

## 五、提交与推送

```powershell
git add -- docs/ARCHITECTURE.md docs/project_management/reports/WB-002_REPORT.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs(WB-002): define MVP architecture"
git push -u origin HEAD:workbuddy/wb-002-architecture
$workbuddyLocalCommit = git rev-parse HEAD
$workbuddyRemoteRef = git ls-remote --heads origin workbuddy/wb-002-architecture
Write-Output $workbuddyLocalCommit
Write-Output $workbuddyRemoteRef
git status --short --branch
```

本地提交号必须等于远端提交号。push 被拒绝时停止并返回原始错误，不得使用 force、rebase、reset 或删除远端分支。

## 六、复检回执

```text
任务 ID：WB-002
状态声明：REVIEW_READY / BLOCKED
分支：workbuddy/wb-002-architecture
开始提交：<开始前 HEAD>
本地 HEAD：<提交后 HEAD>
远端 HEAD：<ls-remote>
实际修改文件：<列表>
git diff --check：PASS/FAIL
12个必需章节：<逐项 PASS/FAIL>
证据标签检查：<结果>
JSON 示例检查：<命令和结果>
Markdown 链接检查：<结果>
范围偏差：无/有
未决架构选择：<清单>
HARDWARE_VERIFY_REQUIRED：<清单>
建议 Codex 复检重点：<清单>
```

完成后停止。WorkBuddy 无权标记 `ACCEPTED`、合并 `main`、修改看板或开始任何代码实现。

## 七、Codex 复检入口

```powershell
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-002-architecture
git diff --check origin/main...origin/workbuddy/wb-002-architecture
git diff --name-status origin/main...origin/workbuddy/wb-002-architecture
git log --oneline --decorate origin/main..origin/workbuddy/wb-002-architecture
```

Codex 将检查事实标签、模块依赖、状态机、数据契约、异常路径、隐私安全、链接和范围，再给出 `ACCEPTED`、`CHANGES_REQUIRED` 或 `BLOCKED`。
