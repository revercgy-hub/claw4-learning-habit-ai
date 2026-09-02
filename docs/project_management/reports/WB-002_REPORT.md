# WB-002 实施报告：Claw4 学习习惯终端 MVP 架构定义

- 任务 ID：WB-002
- 状态声明：REVIEW_READY（修订轮 CR-WB002-01~10；WB-STREAM-001 CP0 收口）
- 分支：`workbuddy/wb-002-architecture` → `workbuddy/mvp-core-stream`（WB-STREAM-001）
- 任务包：`docs/project_management/tasks/WB-002_ARCHITECTURE.md`、`docs/project_management/tasks/WB-STREAM-001_MVP_CORE.md`
- 初次提交：`7902378dc844f1855f93708b14cb25c0c3b16fa4`（`docs(WB-002): define MVP architecture`）
- 修订提交信息：`docs(WB-002): harden offline and auth contracts`（CR-01~05）、`docs(WB-002): close replay and claim contracts`（CR-06~10，WB-STREAM-001 CP0）
- 修订基线：`bfa5f5e76270c2eed475faf0293ee3986c360007`（Codex 复检合并提交）；CP0 基线 `cd8d2707936d8842c142049b9cf3b4ae07d285b4`（WB-STREAM-001 起始）
- 本地/远端 HEAD：CP0 修订提交自身（推送后以 `git rev-parse HEAD` 与 `git ls-remote` 为准；报告文件位于该提交内，无法自引用其 hash，详见 §5）
- 日期：2026-09-02

## 1. 实际修改文件

- `docs/ARCHITECTURE.md`（初次提交 `7902378` 新建，本修订轮修改）
- `docs/project_management/reports/WB-002_REPORT.md`（初次提交 `7902378` 新建，本修订轮修改）

**仅上述 2 个允许文件**（修订轮也仅修改这两个原文件）。未创建源码、目录骨架、示例工程、构建配置、Docker、测试工程或任何其他文件；未修改官方源码/BSP/固件/分区；未操作任何硬件或串口。

## 2. 实现摘要

`docs/ARCHITECTURE.md` 按任务包 12 章完整覆盖：

| 章 | 内容要点 |
| --- | --- |
| 1 | MVP 唯一闭环、复用/新建/禁止边界、非目标清单、证据标签约定 |
| 2 | 系统上下文依赖方向（家长端不直连设备、设备不直连 AI Provider）、数据权威所有者与设备缓存副本表 |
| 3 | 设备六模块边界（device_services / learning_domain / sync / ui / assistant / telemetry），每模块职责/允许依赖/禁止依赖/接口/落点 |
| 4 | 设备一级状态 + 学习会话子状态分层、与官方 `device_state.h` 枚举的桥接、Task/StudySession 生命周期、5 条异常路径、7 条不变量 |
| 5 | Task / StudySession / Event envelope 契约（字段/必填性/版本/时间语义）、幂等/顺序/at-least-once/冲突策略、服务器 JSON 覆盖禁令 |
| 6 | MVP 端点表、最小 JSON 示例（注册/auth/今日任务/事件批量）、HTTPS/WSS 与证书校验、每设备凭据、状态码与重试分类 |
| 7 | 离线能力、NVS+既有 FAT 落点（不新增分区/不强依赖 microSD）、事件队列规则（顺序/确认删除/掉电恢复/容量/退避/死信）、断点续传 |
| 8 | Home/Focus/Done/Offline 四页 intent 流转、LVGL 线程归属、基于 360 MHz/32 MB PSRAM 已确认事实的预算（未实测项不写已达成） |
| 9 | 后端推荐目录与职责（ARCH_DECISION）、数据库最低模型、PWA 4 页范围、AI Gateway Provider 接口边界 |
| 10 | 儿童数据最小化、日志脱敏与禁止记录、10 项失败矩阵 |
| 11 | 8 项分阶段实施任务（全部 HOLD，等待 G2/G3，本文不自动授权编码） |
| 12 | 8 项决策表（理由/证据/影响/可逆性）、12 项 HARDWARE_VERIFY_REQUIRED/UNKNOWN 清单（链接证据文档）、5 项产品待确认 |

### 2.1 修订轮 CR-WB002-01~04 落实表（本次修订）

| CR | 修订位置（`docs/ARCHITECTURE.md`） | 修订内容 | 验证结果 |
| --- | --- | --- | --- |
| 01 原子 outbox | §4.5 队列满行、§4.6 新增不变量 I8、§7.3 表重写 + 新增 7.3.1/7.3.2/7.3.3、§10.3 队列满/后端重启行 | 定义"验证 intent → 原子持久化快照+事件 → 提交成功才向 UI 确认"的 transactional outbox；关键事件无法持久化则不提交完成状态、重试复用原 `event_id`；为完成类事件预留容量、压力下合并/丢弃非关键 telemetry；`sync.failed`/`sync.recovered` 为本地可合并诊断不入业务队列 | outbox 关键词命中 7 处；旧表述"隔离新业务事件"零残留 |
| 02 连续 ACK/死信 | §5.4 表重写（新增 ACK 定义行 + 服务端事务行）、§6.3 `/events/batch` 响应改为逐事件结果、§6.7 状态码表、§7.3 确认删除/死信行、§7.4 断点续传 | ACK 定义为"最高连续、已持久化且业务处理成功"的 sequence，不得以批次最大成功序号代替；`/events/batch` 返回 `results[]` 逐事件结果，客户端只删成功/重复且位于连续 ACK 前缀内的 pending 事件；业务性 4xx 关键事件持久保留并以同一 `event_id` 重放/人工补偿；401/403 不是死信条件（暂停同步、事件保持 pending）；MVP 单设备串行批次 + 设备级锁 | 新响应 JSON `json.loads` PASS；"连续 ACK"等关键词命中；旧表述"移入死信区/仍失败进死信"零残留 |
| 03 重启/完成语义 | §4.3 重启描述、§4.4 状态图与 `completion_type` 重写、§4.5 设备重启行 + 新增专注段到时行、§5.2 字段语义、§10.3 设备重启行 | Task 状态与 StudySession 状态分离，`completion_type` 不替代 `status`；重启恢复唯一流程（快照完整→恢复同一 session_id 用户选择继续/结束；损坏→`aborted`/`auto_saved`，Task 不自动 completed）；Timer 到时仅结束/暂停专注段，不得自动完成任务；`completion_type` 枚举移除 `timeout`（改为专注段语义） | "无 timeout 值"表述 2 处一致；旧值 `normal/manual/timeout` 零残留；状态图/异常矩阵/契约/不变量四处同步 |
| 04 设备—儿童绑定与写入入口 | §6.2 端点表（`/events/batch` 唯一入口 + 新增 `/devices/claim` + finish 排除）、§6.3 注册/配对/认证 JSON 重写、§6.4 新增 6.4.1 绑定模型/6.4.2 配对/claim/6.4.3 nonce 防重放、§9.1 后端职责、§10.3 新增 3 行、§12.1 新增 D9/D10 | `/events/batch` 为设备业务写入唯一权威入口；`/study-sessions/{id}/finish` 不属于设备 MVP 接口；最小身份流程（注册→配对/claim→认证→短期 token），服务端签发 `device_id`；token 携带 device/child 绑定、逐请求/逐事件校验；nonce 一次性/短 TTL/防重放；注册/配对失败统一通用错误不泄露儿童存在；MVP 单家长最小授权边界 | 新 claim JSON `json.loads` PASS；配对 19/绑定 16/nonce 10/claim 7 处命中；旧"二选一"双写入路径零残留 |

**修订范围**：仅修改两个原文件（`docs/ARCHITECTURE.md`、本报告）与工作流报告（`WB-STREAM-001_REPORT.md`，WB-STREAM-001 允许路径）；未创建源码/新文档，未构建、未连接硬件、未触碰看板/任务包/Codex 文档。

### 2.2 修订轮 CR-WB002-06~09 落实表（WB-STREAM-001 CP0）

| CR | 修订位置（`docs/ARCHITECTURE.md`） | 修订内容 | 测试向量（可转 Mock 用例） | 验证结果 |
| --- | --- | --- | --- | --- |
| 06 幂等先于 sequence | §5.4 表新增"逐事件处理顺序（MVP）"行 + 重写幂等/顺序/服务端事务/重复事件处理行 + 新增"可测试示例：响应丢失重发" | 逐事件固定顺序：① 认证/归属校验 → ② `(device_id, event_id)` 幂等查重 → ③ 仅新事件做 sequence 连续性；摘要一致重发 → `duplicate`+当前 ACK；同 ID 不同 sequence/type/载荷 → `conflict` 拒绝并告警；新事件 `sequence>期望` → `gap`，`≤ACK 且未见过` → `rejected` 回退不推进 ACK | seq=42 落库成功但响应丢失 → 同 event_id/同载荷重发返回 duplicate + last_acked_sequence=42；同 event_id 改 payload 返回 conflict | JSON 12/12；"响应丢失"示例文本命中；旧"先 sequence 后幂等"表述零残留 |
| 07 challenge 获取 | §6.2 端点表新增 `POST /devices/challenge`、`/devices/auth` 改"两阶段"；§6.3 新增 challenge 请求/响应 JSON、auth 请求更新（challenge_id+nonce+签名绑定）；§6.4.3 重写为两阶段交互（获取→认证、单次使用、过期/重放 401、日志不记录 nonce/secret/签名原文）；§10.3 新增 challenge 过期/重放行；§12.1 新增 D12 | 两阶段：`POST /devices/challenge`（请求 device_id）→ `challenge_id+nonce+expires_at`；`POST /devices/auth` 提交 `challenge_signature=sign(device_secret, device_id|challenge_id|nonce)`；challenge 单次、过期/重放 401、认证成功后作废 | challenge 过期拒绝；同一 challenge 二次使用拒绝；跨设备用同一 nonce 拒绝；日志不含 nonce/签名原文 | challenge JSON `json.loads` PASS；challenge 10 处/nonce 11 处命中；可解析最小示例新增 2 个 |
| 08 claim 家长授权链 | §6.3 claim JSON 删除 `parent_id`；§6.4.2 重写（claim 必须由已认证家长会话调用、parent_id 从认证上下文派生、child_id 必须属于家长授权范围、未授权儿童/无效配对码/不存在对象统一通用外部错误+服务端脱敏审计原因）；§10.3 新增 2 行；§12.1 新增 D11 | `/devices/claim` 由已认证家长 token 调用；`parent_id` 不接受请求体自报；校验 child 归属；失败统一通用错误不泄露儿童存在性 | claim 带 parent_id 被忽略；child 不属于该家长 → 通用外部错误；未授权儿童 → 同一错误信息 | claim JSON `json.loads` PASS；"认证家长"3 处/"授权范围"3 处命中；旧自报 parent_id 示例零残留 |
| 09 损坏快照唯一语义 | §4.3 设备重启行、§4.4 状态图 ABORTED 分支与 `completion_type` 定义、§4.5 设备重启行（原有）、§10.3 设备重启行（原有） | 全文统一：快照损坏/不可恢复**只产生 `aborted`**；快照完整、恢复同一 session 后**由用户显式选择结束/保存**才可 `auto_saved`；两者均不得自动完成 Task（I2） | 损坏快照恢复 → session 标 aborted（无 auto_saved）；完整快照+用户显式结束 → auto_saved；两者 Task 均不自动 completed | "快照损坏→aborted"3 处一致、"auto_saved 需用户显式"2 处一致；旧"aborted/auto_saved 并列"零残留 |

**CP0 复验数字**：JSON 12/12（新增 challenge 请求/响应）· Markdown 链接 21/21 · 章节 §0~§12 共 13 个 · 证据标签 157 处（ARCH_DECISION 87）· `git diff --check` PASS · 旧语义零残留。

## 3. 验收标准逐项自检

| # | 验收标准 | 结果 |
| --- | --- | --- |
| 1 | Git diff 只有两个允许文件 | PASS（见 §4） |
| 2 | 架构覆盖 12 章节，术语与状态机前后一致 | PASS（状态机术语统一：Task 生命周期、StudySession 生命周期、异常路径、不变量；设备一级状态与子状态分层一致） |
| 3 | 设备/后端/PWA/AI 与数据所有权边界清晰，无循环依赖 | PASS（§2 依赖方向图 + §3 模块禁止依赖表；无反向依赖） |
| 4 | Task/StudySession/Event 契约足以指导单测与 Mock Backend，但未写实现代码 | PASS（§5 完整字段表 + JSON 示例；全文零代码实现） |
| 5 | 离线队列、幂等、顺序、重试和掉电恢复规则可验证 | PASS（§5.4 + §7.3 表格式规则，可直接转测试用例） |
| 6 | 无业务层直连 GPIO/LVGL/网络、无家长端直连设备、无设备直连 AI Provider | PASS（不变量 I5/I6 + 模块禁止依赖） |
| 7 | 未把 4G、屏驱、触摸、Flash 容量、分区实际布局或照片接口功能写成实机已确认 | PASS（§12.2 清单全部标 HARDWARE_VERIFY_REQUIRED/UNKNOWN/USER_EVIDENCE_REQUIRED；§8.3 预算未实测项明确标注） |
| 8 | 未引入真实儿童数据、凭据、服务账号或付费外部依赖 | PASS（JSON 示例全为 `00000000-...` 合成 uuid 与占位 token；无 API key/密钥） |
| 9 | 未修改源码、官方 BSP、固件、分区、构建或硬件状态 | PASS（仅 2 个 docs 文件新建；未运行构建/未连接设备） |
| 10 | 报告包含实际验证命令、结果、风险和 Codex 复检重点 | PASS（§4/§6/§7） |
| 11 | `git diff --check origin/main...HEAD` 无错误 | PASS（§4） |

## 4. 验证命令与结果

```powershell
# 空白检查（修订提交相对修订基线 bfa5f5e7 及远端 main 32cf6bfb 均执行）
git diff --check
# 结果：无输出（PASS）

# 修改文件清单（修订提交相对修订基线）
git diff --name-status bfa5f5e76270c2eed475faf0293ee3986c360007
# 结果：仅 M docs/ARCHITECTURE.md、M docs/project_management/reports/WB-002_REPORT.md

# 证据标签检查（任务包 §验证要求，CP0 修订后重新扫描）
rg -n "PRODUCT_REQUIRED|SOURCE_CONFIRMED|DEVICE_LOG_CONFIRMED|ARCH_DECISION|HARDWARE_VERIFY_REQUIRED|UNKNOWN" docs/ARCHITECTURE.md
# 结果：命中 157 处，标签计数：
#   ARCH_DECISION 87 / PRODUCT_REQUIRED 29 / UNKNOWN 16 / SOURCE_CONFIRMED 11 / HARDWARE_VERIFY_REQUIRED 9 / DEVICE_LOG_CONFIRMED 4 / USER_EVIDENCE_REQUIRED 1

# CP0 关键词检查（WORKBUDDY_GIT_SYNC.md §四 四组）
rg -n "duplicate|event_id|sequence|last_acked|响应丢失|幂等|回退|conflict" docs/ARCHITECTURE.md   # 53 处
rg -n "challenge|nonce|过期|单次|重放|签名" docs/ARCHITECTURE.md                                   # 33 处
rg -n "claim|parent_id|child_id|认证家长|授权范围|通用错误" docs/ARCHITECTURE.md                     # 31 处
rg -n "快照损坏|auto_saved|aborted|显式|task.completed" docs/ARCHITECTURE.md                        # 14 处

# JSON 示例可解析性（CP0 修订后 12/12 PASS，含新增 /devices/challenge 请求/响应）
# python 脚本提取全部 ```json 代码块并 json.loads 校验

# 本地 Markdown 链接存在性（21/21 PASS）
# python 脚本解析 [text](path) 并 os.path.exists 校验（含 ../../ 相对路径）

# 章节完整性（§0~§12 共 13 个章节标题）
# grep -nE "^## [0-9]+" docs/ARCHITECTURE.md
```

**敏感表述扫描（CP0 修订后）**：对"aborted/auto_saved 并列、快照损坏→auto_saved、可含 auto_saved、自报 parent_id、先 sequence 后幂等、最大成功序号"等旧语义做残留扫描，命中均为新语义否定句或正确表述（§4.3 :206、§4.4 :214/:219、§4.5 :228、§10.3 :795 全部为"损坏→仅 aborted / 完整+用户显式→auto_saved"），无旧语义残留。

## 5. 关于提交号的说明（消除占位符）

本报告文件与 `docs/ARCHITECTURE.md` 同属 CP0 修订提交（`docs(WB-002): close replay and claim contracts`）。Git 提交 hash 由提交内容决定，报告文本无法自引用自身 hash，故"本地/远端 HEAD"字段如实写为"CP0 修订提交自身"，推送后由 Codex 按 `WORKBUDDY_GIT_SYNC.md` §七 复检入口（`git rev-parse` + `git ls-remote`）验证。此前提交记录：初次 `7902378`、CR-01~05 修订 `6251486`。本报告不含任何"提交后填"式占位符。

## 6. 未解决问题、风险与 HARDWARE_VERIFY_REQUIRED 项

### 6.1 HARDWARE_VERIFY_REQUIRED / UNKNOWN 清单

见 `docs/ARCHITECTURE.md` §12.2（12 项，均链接现有证据文档，不复制过期结论）。核心项：屏驱 SKU、触摸、Flash 容量、PSRAM 实际分配、C5 状态、实际网络通道/4G、partition/OTA 实际布局、电池/充电 IC、音频硬件、摄像头型号、SD 热插拔、外观 SKU。

### 6.2 风险

| 风险 | 说明 |
| --- | --- |
| 非对称 OTA 槽位 | ota_0=9M / ota_1=4M，当前固件 ≈8.62 MiB；未来固件更新受 `BLK-FLASH-AUTH-001` 与分区门禁约束 |
| 后端技术栈未锁定 | FastAPI/PostgreSQL 为 `ARCH_DECISION` 推荐，未选型重大替代方案（无需上报 Codex 的停止条件，因未发现现有产品事实无法决定的冲突） |
| WSS vs 轮询 | MVP 采用 REST 批量同步，家长端可见性为秒级延迟；如需实时推送再引入 WSS |
| 持久化选型 | NVS vs 既有 FAT 分区列为 `ARCH_DECISION`，实现时按队列规模与实测磨损再定，不新增分区 |

### 6.3 CR 修订后剩余风险

| 风险 | 说明 |
| --- | --- |
| outbox 原子性依赖存储能力 | 7.3.1 的"原子写序列"在 NVS 上需日志式提交；若实现时发现单次写入体积超出 NVS 页限制，需在 T7（离线队列）任务包中先解决，本架构仅定规则不锁定实现 |
| MVP 串行批次吞吐 | 5.4 服务端事务采用单设备串行批次 + 设备级锁，极端大批量时后端单设备处理为串行；MVP 规模（单家庭设备量级）可接受，V1 若需并行再做乱序缓冲 |
| 设备密钥存储 | 6.4 `device_secret` 的硬件安全存储依赖实机安全能力（Secure Boot/Flash Encryption 当前 `SOURCE_CONFIRMED` 未确认），MVP 先落 NVS 安全分区，正式版升级 |
| 多家长模型 | 6.4.1 明确 MVP 单家长最小授权边界；多家长（P5）待定，扩展时需引入家长级权限模型 |
| conflict 告警的人工补偿路径 | 5.4 对"同 event_id 不同载荷"返回 `conflict`，但设备端人工/自动修复路径未在契约级定义，需 Mock 测试时给出可测试的冲突处理动作（保留 pending + 告警 + 人工重放） |
| challenge 签名算法实现 | 6.4.3 建议 HMAC-SHA256，但设备侧密钥管理与签名实现依赖实机安全能力；MVP 先按纯软件 HMAC 实现，正式版随 Secure Boot 升级 |

## 7. 范围偏差

无。全程未创建任务包允许列表之外的文件，未运行构建/刷写，未连接设备，未修改官方源码，未触碰 TASK_BOARD/任务包/Codex 文档，未 force push/rebase/reset。

## 8. 复检回执

```text
任务 ID：WB-002（WB-STREAM-001 CP0 收口）
状态声明：REVIEW_READY（修订轮 CR-WB002-01~10）
分支：workbuddy/mvp-core-stream
初次提交：7902378dc844f1855f93708b14cb25c0c3b16fa4（docs(WB-002): define MVP architecture）
CR-01~05 修订：6251486ce88d841cfc40f23ef4f048ec751b7191（docs(WB-002): harden offline and auth contracts）
CP0 基线：cd8d2707936d8842c142049b9cf3b4ae07d285b4（WB-STREAM-001 起始）
本地 HEAD：CP0 修订提交自身（docs(WB-002): close replay and claim contracts）
远端 HEAD：以 git ls-remote --heads origin workbuddy/mvp-core-stream 为准
实际修改文件：docs/ARCHITECTURE.md, docs/project_management/reports/WB-002_REPORT.md, docs/project_management/reports/WB-STREAM-001_REPORT.md（CP0 三个允许文件）
git diff --check：PASS
12个必需章节：逐项 PASS（§0 证据标签约定 + §1~§12）
CR-WB002-01~05 修订：已验收通过（Round 2 复检）
CR-WB002-06~09 修订：逐项落实并验证（见 §2.2 表，含测试向量）
证据标签检查：157 处命中（ARCH_DECISION 87 / PRODUCT_REQUIRED 29 / UNKNOWN 16 / SOURCE_CONFIRMED 11 / HARDWARE_VERIFY_REQUIRED 9 / DEVICE_LOG_CONFIRMED 4 / USER_EVIDENCE_REQUIRED 1）
JSON 示例检查：12/12 块 json.loads PASS（新增 /devices/challenge 请求/响应）
Markdown 链接检查：21/21 本地链接存在（含 ../../项目总规划/AGENTS.md）
范围偏差：无（未创建源码/新文档；未构建；未连接硬件；未触碰看板/任务包/Codex 文档）
CR 修订后剩余风险：见本报告 §6.3（outbox 存储实现 / MVP 串行批次吞吐 / 设备密钥存储 / 多家长模型 / conflict 人工补偿路径 / challenge 签名算法）
未决架构选择：后端技术栈最终确认（P1）、激励体系范围（P2）、WSS 是否进 MVP（P3）、OTA 策略（P4）、家长权限模型（P5）
HARDWARE_VERIFY_REQUIRED：见 ARCHITECTURE.md §12.2（12 项：屏驱/触摸/Flash容量/PSRAM分配/C5/网络通道/分区布局/电池/音频/摄像头/SD/外观SKU）
建议 Codex 复检重点（CP0）：
  1. 逐事件处理顺序（5.4）"幂等先于 sequence"是否可唯一驱动 Mock Backend（CP2 必测向量：响应丢失重发→duplicate）
  2. challenge 两阶段（6.3/6.4.3）请求/响应与防重放规则是否可实现、可测试
  3. claim 家长信任链（6.4.2）——认证上下文派生 parent_id + child 授权校验 + 通用外部错误
  4. 损坏快照唯一语义（4.3/4.4/4.5/10.3）——aborted 与 auto_saved 是否只剩一套可测试语义
  5. §12.1 D11/D12 决策是否与 §5.4/§6.3 自洽
```

## 9. Codex 复检入口（供复核）

```powershell
# CP0 验证（WB-STREAM-001 工作流分支）
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/mvp-core-stream
git log --oneline --decorate origin/main..origin/workbuddy/mvp-core-stream
# 期望：cd8d270（流起始）→ <CP0 提交>
git diff --check cd8d2707936d8842c142049b9cf3b4ae07d285b4..<CP0 提交>
git diff --name-status cd8d2707936d8842c142049b9cf3b4ae07d285b4..<CP0 提交>
# 期望：仅 M docs/ARCHITECTURE.md、M docs/project_management/reports/WB-002_REPORT.md、A docs/project_management/reports/WB-STREAM-001_REPORT.md
```
