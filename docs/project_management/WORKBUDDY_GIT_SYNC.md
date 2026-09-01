# WorkBuddy Git 同步与复检交付指令

## 当前任务

- 项目远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 任务：`WB-002`
- 状态：`CHANGES_REQUIRED`
- 分支：`workbuddy/wb-002-architecture`
- 最新复检：`docs/project_management/reports/CODEX_REVIEW_WB-002_2026-09-02.md`
- 允许修改：
  - `docs/ARCHITECTURE.md`
  - `docs/project_management/reports/WB-002_REPORT.md`

本轮只修订架构契约。不得创建源码、构建配置、测试工程或额外文档，不得构建、下载依赖、操作硬件、串口、Flash、固件或外部服务。

## 一、同步原分支

```powershell
git status --short --branch
git remote -v
git fetch --prune origin
git ls-remote --heads origin main workbuddy/wb-002-architecture
git switch workbuddy/wb-002-architecture
git pull --ff-only origin workbuddy/wb-002-architecture
git status --short --branch
git rev-parse HEAD
git rev-parse origin/workbuddy/wb-002-architecture
```

若本地无该分支：

```powershell
git switch --track -c workbuddy/wb-002-architecture origin/workbuddy/wb-002-architecture
```

继续条件：远端 URL 正确、本地 HEAD 等于远端任务分支、工作区干净、当前目录不是 `vendor/MetalioClaw4`。任何不一致立即停止；不得 force、reset、rebase、删除分支或覆盖文件。

## 二、必须读取

1. 根 `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/TASK_BOARD.md`
4. `docs/project_management/tasks/WB-002_ARCHITECTURE.md`
5. `docs/project_management/reports/CODEX_REVIEW_WB-002_2026-09-02.md`
6. 现有 `docs/ARCHITECTURE.md`
7. 现有 `docs/project_management/reports/WB-002_REPORT.md`

## 三、唯一修订范围

逐项完成复检报告的 CR-WB002-01～05：

1. 增加领域状态快照与关键事件的 transactional outbox/等价原子持久化规则；队列满时不能先完成业务再丢事件。
2. 重构 ACK/死信：ACK只能推进到最高连续成功序号；批响应含逐事件结果/缺口；关键4xx事件保持可修复重放；认证失败不把业务事件放入死信。
3. 统一 Task、StudySession、重启恢复、timeout、auto_saved 语义；Timer 到时不能自动完成 Task。
4. `/events/batch` 作为设备业务写入唯一权威入口；补齐注册、家长配对、认证、token中的 device-child绑定、nonce防重放及服务端归属校验。
5. 更新报告，逐项列出修改位置与实际验证结果。

不得改变 MVP 范围、解除 G2/G3、决定 OTA/分区、添加产品功能或开始实现。

## 四、验收前自检

```powershell
git status --short
git diff --check
git diff --name-only
git diff -- docs/ARCHITECTURE.md docs/project_management/reports/WB-002_REPORT.md
rg -n "transactional outbox|原子|关键事件|highest_contiguous|连续|rejected|dead.?letter|死信|same event_id|同一 event_id" docs/ARCHITECTURE.md
rg -n "Task|StudySession|timeout|auto_saved|aborted|重启|显式|task.completed" docs/ARCHITECTURE.md
rg -n "pair|配对|claim|device_id|child_id|nonce|重放|events/batch|study-sessions" docs/ARCHITECTURE.md
```

必须满足：

- 仅两个允许文件；
- 关键状态转换与事件持久化具有明确原子顺序；
- 队列满、4xx、401/403和ACK跳号均不能静默丢失关键事件；
- 同一事件重试保留同一 `event_id`；
- 重启与Timer到时都不会自动把 Task 标为完成；
- token绑定的设备/儿童由服务端校验，不信任载荷自报身份；
- 设备业务写入只有一个权威路径；
- 9个JSON示例或修订后全部JSON块均可解析，Markdown本地链接存在；
- `git diff --check` 无输出。

## 五、提交与推送

```powershell
git add -- docs/ARCHITECTURE.md docs/project_management/reports/WB-002_REPORT.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs(WB-002): harden offline and auth contracts"
git push -u origin HEAD:workbuddy/wb-002-architecture
$workbuddyLocalCommit = git rev-parse HEAD
$workbuddyRemoteRef = git ls-remote --heads origin workbuddy/wb-002-architecture
Write-Output $workbuddyLocalCommit
Write-Output $workbuddyRemoteRef
git status --short --branch
```

本地提交必须等于远端提交。push被拒绝时停止并返回原始错误；禁止force、rebase、reset或删除远端分支。

## 六、复检回执

```text
任务 ID：WB-002
状态声明：REVIEW_READY / BLOCKED
分支：workbuddy/wb-002-architecture
初次提交：7902378dc844f1855f93708b14cb25c0c3b16fa4
本地 HEAD：<修订提交>
远端 HEAD：<ls-remote>
CR-WB002-01：<修改位置、规则、验证>
CR-WB002-02：<修改位置、ACK/死信验证>
CR-WB002-03：<修改位置、状态转换验证>
CR-WB002-04：<修改位置、授权绑定验证>
CR-WB002-05：<报告与全量校验>
实际修改文件：<列表>
JSON检查：<块数与结果>
Markdown链接检查：<数量与结果>
git diff --check：PASS/FAIL
本轮源码/构建/硬件操作：0
范围偏差：无/有
剩余风险：<清单>
```

完成后停止。WorkBuddy不得自行标记 `ACCEPTED`、修改看板、合并main或开始实现。

## 七、Codex Round 2 入口

```powershell
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-002-architecture
git diff --check origin/main...origin/workbuddy/wb-002-architecture
git diff --name-status origin/main...origin/workbuddy/wb-002-architecture
git log --oneline --decorate origin/main..origin/workbuddy/wb-002-architecture
```
