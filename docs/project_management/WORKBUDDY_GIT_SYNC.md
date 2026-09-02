# WorkBuddy Git 同步与复检交付指令

## 当前任务

- 项目远端：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`
- 任务：`WB-002`
- 状态：`CHANGES_REQUIRED`
- 分支：`workbuddy/wb-002-architecture`
- 最新复检：`docs/project_management/reports/CODEX_REVIEW_WB-002_ROUND2_2026-09-02.md`
- 允许修改：
  - `docs/ARCHITECTURE.md`
  - `docs/project_management/reports/WB-002_REPORT.md`

本轮只处理 CR-WB002-06～10。不得创建源码、构建配置、测试工程或额外文档，不得构建、下载依赖、操作硬件、串口、Flash、固件或外部服务。

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
5. `docs/project_management/reports/CODEX_REVIEW_WB-002_ROUND2_2026-09-02.md`
6. 现有 `docs/ARCHITECTURE.md`
7. 现有 `docs/project_management/reports/WB-002_REPORT.md`

## 三、唯一修订范围

逐项完成复检报告的 CR-WB002-06～10：

1. 明确逐事件处理顺序：认证/归属校验后先按 `(device_id,event_id)` 查重；相同内容返回 `duplicate`，同 ID 不同内容拒绝；只对未见过的新事件执行连续 sequence 检查。
2. 增加“服务端已接受 seq=42、响应丢失、设备用同 event_id 重发”的测试向量，确保返回 duplicate/当前连续 ACK，客户端可安全删除。
3. 定义 nonce/challenge 获取调用或清晰的两阶段 auth；补齐 challenge JSON、过期、单次使用、签名绑定和重放失败规则。
4. claim 必须由已认证家长调用；请求体删除 `parent_id`，服务端从认证上下文派生家长并校验 `child_id` 归属。
5. 统一快照损坏只为 `aborted`；完整快照恢复后由用户结束/保存才可 `auto_saved`；两者均不自动完成 Task。
6. 更新报告，逐项列出 CR-WB002-06～10 的修改位置、测试向量、验证结果与剩余风险。

不得改变 MVP 范围、解除 G2/G3、决定 OTA/分区、添加产品功能或开始实现。

## 四、验收前自检

```powershell
git status --short
git diff --check
git diff --name-only
git diff -- docs/ARCHITECTURE.md docs/project_management/reports/WB-002_REPORT.md
rg -n "duplicate|event_id|sequence|last_acked|响应丢失|幂等|回退|冲突" docs/ARCHITECTURE.md
rg -n "challenge|nonce|过期|单次|重放|签名" docs/ARCHITECTURE.md
rg -n "claim|parent_id|child_id|认证家长|授权范围|通用错误" docs/ARCHITECTURE.md
rg -n "快照损坏|auto_saved|aborted|显式|task.completed" docs/ARCHITECTURE.md
```

必须满足：

- 仅两个允许文件；
- 同一事件重试保留同一 `event_id`，先幂等查重再对新事件做 sequence 连续性检查；
- 响应丢失后的重复投递返回成功语义，不造成永久 pending；
- nonce/challenge 有完整可调用流程、单次使用和过期/重放规则；
- claim 不信任请求体自报 parent_id，儿童归属由已认证家长上下文校验；
- 损坏快照只为 `aborted`，`auto_saved` 只用于完整快照恢复后的用户结束/保存；
- 已通过的 outbox、连续 ACK、唯一写入口与 Task 显式完成规则不得回退；
- 9个JSON示例或修订后全部JSON块均可解析，Markdown本地链接存在；
- `git diff --check` 无输出。

## 五、提交与推送

```powershell
git add -- docs/ARCHITECTURE.md docs/project_management/reports/WB-002_REPORT.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs(WB-002): close replay and claim contracts"
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
修订基线：6251486ce88d841cfc40f23ef4f048ec751b7191
CR-WB002-06：<幂等优先级、响应丢失测试向量、验证>
CR-WB002-07：<challenge 获取、签名绑定、过期/重放验证>
CR-WB002-08：<认证家长上下文、parent_id 派生、child 归属验证>
CR-WB002-09：<aborted/auto_saved 唯一语义与残留扫描>
CR-WB002-10：<报告与全量校验>
实际修改文件：<列表>
JSON检查：<块数与结果>
Markdown链接检查：<数量与结果>
git diff --check：PASS/FAIL
本轮源码/构建/硬件操作：0
范围偏差：无/有
剩余风险：<清单>
```

完成后停止。WorkBuddy不得自行标记 `ACCEPTED`、修改看板、合并main或开始实现。

## 七、Codex Round 3 入口

```powershell
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-002-architecture
git diff --check origin/main...origin/workbuddy/wb-002-architecture
git diff --name-status origin/main...origin/workbuddy/wb-002-architecture
git log --oneline --decorate origin/main..origin/workbuddy/wb-002-architecture
```
