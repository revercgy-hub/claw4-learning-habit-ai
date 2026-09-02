# WB-STREAM-001 工作流报告：MVP 核心连续开发

- 工作流 ID：WB-STREAM-001
- 分支：`workbuddy/mvp-core-stream`
- 任务包：`docs/project_management/tasks/WB-STREAM-001_MVP_CORE.md`
- 起始提交（远端任务分支 HEAD）：`cd8d2707936d8842c142049b9cf3b4ae07d285b4`
- 执行方式：CP0 → CP1 → CP2 连续执行，checkpoint 之间不等待 Codex
- 状态：`CHECKPOINT_READY`（按 checkpoint 更新）
- 更新日期：2026-09-02

## 1. 工作流状态表

| Checkpoint | 状态 | 提交 | 验证摘要 |
| --- | --- | --- | --- |
| CP0 架构契约收口（CR-WB002-06~10） | `CHECKPOINT_READY` | `72b77ee9cd328bccf27fecbdf32194ab12505e00`（`docs(WB-002): close replay and claim contracts`） | JSON 12/12、链接 21/21、章节 13、标签 157、旧语义零残留、`git diff --check` PASS |
| CP1 设备侧接口骨架 | `CHECKPOINT_READY` | `a38dfad3b5d307e734af0b82999151c7878092ef`（`feat(WB-STREAM-001): add MVP interface contracts`） | 依赖扫描 7 头零硬件依赖、16/16 头 `-fsyntax-only` PASS、契约测试 1/1 PASS（P4 交叉编译器） |
| CP2 FastAPI Mock Backend | `CHECKPOINT_READY` | `feat(WB-STREAM-001): add contract mock backend`（本提交自身） | **pytest 22/22 PASS**、`git diff --check` PASS、仅绑定 127.0.0.1 |

## 2. CP0：架构契约收口（CR-WB002-06~10）

### 2.1 修改文件（CP0）

- `docs/ARCHITECTURE.md`（修改）
- `docs/project_management/reports/WB-002_REPORT.md`（修改，新增 §2.2 CR-06~09 落实表）
- `docs/project_management/reports/WB-STREAM-001_REPORT.md`（新建，本报告）

**仅上述 3 个允许文件**。未创建源码/目录骨架/构建配置；未修改官方源码/BSP/固件/分区；未操作硬件或串口；未触碰看板/任务包/Codex 文档。

### 2.2 CR-WB002-06~10 落实摘要

| CR | 修订 |
| --- | --- |
| 06 幂等先于 sequence | §5.4 新增"逐事件处理顺序"（认证/归属 → 幂等查重 → 仅新事件做连续性）；摘要一致重发 → `duplicate`+当前 ACK；同 ID 不同载荷 → `conflict`；新增"响应丢失重发"可测试示例 |
| 07 challenge 获取 | 新增 `POST /devices/challenge` 端点与 JSON（challenge_id+nonce+expires_at）；`/devices/auth` 改两阶段（签名绑定 device_id/challenge_id/nonce）；单次使用/过期/重放 401；日志不记录 nonce/secret/签名 |
| 08 claim 家长授权链 | claim JSON 删除 `parent_id`（认证上下文派生）；claim 必须由已认证家长会话调用；child_id 必须属于家长授权范围；失败统一通用外部错误不泄露儿童存在 |
| 09 损坏快照唯一语义 | 快照损坏只产生 `aborted`；快照完整+用户显式结束/保存才 `auto_saved`；两者均不自动完成 Task |
| 10 报告与复验 | 本报告 + WB-002_REPORT §2.2/§4/§6.3/§8 更新；JSON/链接/章节/标签/旧语义/diff 全量复验 |

### 2.3 验证命令与结果（CP0）

```powershell
# 关键词四组扫描（WORKBUDDY_GIT_SYNC.md §四）
rg -n "duplicate|event_id|sequence|last_acked|响应丢失|幂等|回退|conflict" docs/ARCHITECTURE.md   # 53 处
rg -n "challenge|nonce|过期|单次|重放|签名" docs/ARCHITECTURE.md                                   # 33 处
rg -n "claim|parent_id|child_id|认证家长|授权范围|通用错误" docs/ARCHITECTURE.md                     # 31 处
rg -n "快照损坏|auto_saved|aborted|显式|task.completed" docs/ARCHITECTURE.md                        # 14 处

# JSON 全部可解析（12/12，含新增 challenge 请求/响应）
# python 提取全部 ```json 块 json.loads 校验 → 12/12 PASS

# Markdown 本地链接全部存在（21/21）
# python 解析 [text](path) + os.path.exists → 21/21 PASS

# 章节完整性（§0~§12 共 13 个标题）
grep -nE "^## [0-9]+" docs/ARCHITECTURE.md

# 证据标签（157 处）
# ARCH_DECISION 87 / PRODUCT_REQUIRED 29 / UNKNOWN 16 / SOURCE_CONFIRMED 11 / HARDWARE_VERIFY_REQUIRED 9 / DEVICE_LOG_CONFIRMED 4 / USER_EVIDENCE_REQUIRED 1

# 旧语义残留扫描
grep -nE "aborted/auto_saved|快照损坏.*auto_saved|可含 auto_saved|自报 parent_id|先 sequence" docs/ARCHITECTURE.md
# 结果：零残留（命中项均为新语义正确表述）

# Git 校验
git diff --check    # PASS（无输出）
git diff --name-status cd8d270..HEAD  # 仅 3 个允许文件
```

### 2.4 CP0 范围偏差与门禁

- 范围偏差：无。
- 硬件/串口/Flash 操作：0。
- 真实凭据/儿童数据/外部服务：0。
- 未写业务源码、未构建、未接入 Mock/真实后端。

### 2.5 CP0 结论

CP0 完成并推送（`72b77ee9cd328bccf27fecbdf32194ab12505e00`），进入 CP1。

### 2.6 建议 Codex 复检重点（CP0）

1. 逐事件处理顺序（§5.4）"幂等先于 sequence"是否可唯一驱动 CP2 Mock 测试（响应丢失重发 → duplicate）。
2. challenge 两阶段（§6.3/§6.4.3）请求/响应与防重放规则是否可实现、可测试。
3. claim 家长信任链（§6.4.2）——认证上下文派生 parent_id + child 授权校验 + 通用外部错误。
4. 损坏快照唯一语义（§4.3/§4.4/§4.5/§10.3）是否只剩一套可测试语义。
5. §12.1 D11/D12 决策是否与 §5.4/§6.3 自洽。

## 3. CP1：设备侧接口骨架

### 3.1 修改文件（CP1）

- `firmware/main/learning_domain/`（新建：`ids.h`、`task.h`、`study_session.h`、`event.h`、`domain_state.h`、`intents.h`、`services.h`）
- `firmware/main/sync/`（新建：`batch_result.h`、`error_class.h`、`event_sink.h`、`sync_client.h`）
- `firmware/main/ui/`（新建：`view_state.h`、`intent_sink.h`）
- `firmware/main/assistant/`（新建：`command.h`、`command_router.h`）
- `firmware/main/telemetry/`（新建：`logger.h`）
- `firmware/tests/contracts/contract_tests.cpp`（新建，编译期契约检查）
- `tools/dev/verify-interface-contracts.ps1`（新建，`-CompilerPath` 参数化交叉编译检查）
- `docs/project_management/reports/WB-STREAM-001_REPORT.md`（修改，回填 CP0 hash + 本段）

**仅上述允许路径**。未实现任何业务逻辑/网络/持久化；未修改官方源码/BSP/固件/分区；未操作硬件或串口。

### 3.2 接口设计要点

| 模块 | 内容 | 边界约束 |
| --- | --- | --- |
| `learning_domain` | Task/Session 值类型、DeviceEvent envelope、DomainState 只读快照、Intent/IntentResult、TaskService/SessionService/TimerService 纯接口 | **禁止 LVGL/Wi-Fi/GPIO/ESP-IDF/FreeRTOS/BSP 头**（依赖扫描验证）；纯 C++17 标准库 |
| `sync` | EventSink（outbox 持久化接口）、SyncClient（`/events/batch` 契约）、BatchSyncResult（连续 ACK + 逐事件结果）、SyncErrorClass（401/403 非死信） | 不实现网络/持久化；逐事件结果 + 连续 ACK 契约固定 |
| `ui` | ViewState 只读快照、IntentSink（唯一通信口） | 不含 LVGL；只读状态 + 发 intent |
| `assistant` | Command 枚举、CommandRouter 接口 | 不接入 LLM/ASR/TTS；只映射命令→intent |
| `telemetry` | LogTag + 脱敏 Logger 接口 | **禁止 token/secret/nonce/签名/儿童姓名/内容**（接口注释显式声明） |

**契约锚点**（contract_tests.cpp static_assert）：TaskStatus/CompletionType/SessionStatus/EventType/EventOutcome 枚举值稳定；`CompletionType` 恰 4 值（无 Timeout）；ID 与值类型可默认构造/可拷贝；8 个接口抽象、8 个 Mock 具体（可无硬件实现）。

### 3.3 验证命令与结果（CP1）

```powershell
# 1) learning_domain 依赖扫描（仅自引用/标准库，零硬件头）
grep -rniE '#\s*include' firmware/main/learning_domain/ | grep -viE 'learning_domain/|string|cstdint|map|optional|vector'
# 结果：无输出（干净，7 个头文件）

# 2) 逐头文件 -fsyntax-only（P4 交叉编译器）
.\tools\dev\verify-interface-contracts.ps1 -CompilerPath "E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe"
# 结果（out/verify-interface-contracts/verify_result.txt）：
#   headers  : 16 / 16 PASS
#   contract : 1/1 PASS (contract_tests.cpp)
#   RESULT: ALL INTERFACE CONTRACT CHECKS PASS（exit 0）

# 3) Git 校验
git diff --check    # PASS（无输出）
git diff --name-status  # 仅允许路径
```

**交叉编译工具链**：`riscv32-esp-elf-g++ (crosstool-NG esp-14.2.0_20260121) 14.2.0`（ESP32-P4 RISC-V 工具链，`-std=c++17 -fsyntax-only`）。**未运行 C++ 单元测试**（本机无 C++ 编译器，任务包明确禁止声称）；`contract_tests.cpp` 仅为编译期契约检查。

**过程中修复**：① ids.h 的 defaulted `operator==` 为 C++20 特性 → 改 C++17 手写实现；② batch_result.h 的 `EventId` 跨命名空间未限定 → 改 `claw4::domain::EventId` 完整限定；③ contract_tests.cpp 的 constexpr 函数内创建非 literal 类型（std::string 成员）需 C++20 → 改为类型特性 static_assert + 普通函数；④ 验证脚本 `$ErrorActionPreference` 从 Stop 改 Continue（PowerShell 5.1 将原生 stderr 包装为 ErrorRecord 导致终止）。

### 3.4 CP1 范围偏差与门禁

- 范围偏差：无。
- 硬件/串口/Flash 操作：0。
- 真实凭据/儿童数据/外部服务：0。
- 未实现业务逻辑、未构建设备固件、未连接硬件。
- 脚本输出目录 `out/` 已被根 `.gitignore` 忽略，未提交。

### 3.5 CP1 结论

CP1 完成并推送（`a38dfad3b5d307e734af0b82999151c7878092ef`），进入 CP2。

## 4. 建议 Codex 复检重点（CP1）

1. learning_domain 依赖扫描是否彻底（`firmware/main/learning_domain/` 7 头零硬件 include）。
2. sync 契约（BatchSyncResult 连续 ACK + 逐事件结果）是否与 ARCHITECTURE.md §5.4/§6.3 一致，可供 CP2 Mock 直接实现。
3. contract_tests.cpp 的契约锚点（枚举值/接口抽象性）是否覆盖足够。
4. verify-interface-contracts.ps1 的 `-CompilerPath` 参数化与 `out/` 输出是否符合后续 CI/其他主机复用。

## 5. CP2：FastAPI Mock Backend

### 5.1 修改文件（CP2）

- `backend/requirements.txt`（新建，锁定依赖）
- `backend/app/__init__.py`、`backend/app/main.py`（FastAPI 路由 + 逐事件处理）、`backend/app/schemas.py`（Pydantic 契约）、`backend/app/security.py`（HMAC/签名 token/两阶段 challenge）、`backend/app/store.py`（内存存储）
- `backend/tests/__init__.py`、`backend/tests/test_mock_backend.py`（22 项 pytest 契约测试）
- `docs/project_management/reports/WB-STREAM-001_REPORT.md`（修改，回填 CP1 hash + 本段）

**仅上述允许路径**。`.venv`、`__pycache__`、`.pytest_cache` 均由 `.gitignore` 忽略，未提交。

### 5.2 端点与实现要点

| 端点 | 行为 |
| --- | --- |
| `POST /api/v1/devices/register` | **服务端签发** device_id + device_secret + 一次性 pairing_code；日志只记 device_id/model/fw，**不记 secret** |
| `POST /api/v1/devices/challenge` | 两阶段认证第一步：一次性 challenge_id + nonce + expires_at（CR-WB002-07） |
| `POST /api/v1/devices/auth` | 第二步：HMAC-SHA256 签名绑定 device_id/challenge_id/nonce；challenge 单次使用/过期/跨设备 → 401；签发含 device/child 绑定的短期 token |
| `POST /api/v1/devices/claim` | 已认证家长 token 调用；**parent_id 由认证上下文派生**（请求体无此字段，自报被忽略）；child 必须属于家长授权范围，否则通用错误（CR-WB002-08） |
| `GET /api/v1/children/{child_id}/tasks/today` | token 的 device/child 绑定校验；未绑定 child → 403 |
| `POST /api/v1/events/batch` | 设备写入唯一入口；**逐事件顺序：认证/归属 → 幂等查重 → 连续性**（CR-WB002-06）：摘要一致重发→duplicate+当前 ACK；同 ID 不同载荷→conflict；新事件 seq==ACK+1→accepted；>期望→gap；≤ACK 未见过→rejected；gap/失败不推进 ACK |
| `GET /health` | 状态探针 |

**测试与运行只绑定 `127.0.0.1`**：TestClient 进程内；手工运行 `uvicorn app.main:app --host 127.0.0.1`（不启动公网监听）。

### 5.3 验证命令与结果（CP2）

```bash
# 项目局部 venv（不污染 ESP-IDF/系统 Python）
python -m venv backend/.venv
backend/.venv/Scripts/pip install -r backend/requirements.txt

# 全部契约测试
cd backend && ./.venv/Scripts/python.exe -m pytest tests/ -v
# 结果：22 passed in 0.90s（22/22 PASS）
```

**测试覆盖（任务包 §6 必测场景 8 项全映射）**：

| 必测场景 | 对应测试 | 结果 |
| --- | --- | --- |
| 1 register 签发 device_id、日志不泄露 secret | `test_register_issues_device_id_and_secret_not_logged` / `test_register_installation_id_is_unique_each_time` | PASS |
| 2 challenge 过期/复用/跨设备被拒 | `test_challenge_expired_rejected` / `test_challenge_reuse_rejected` / `test_challenge_cross_device_rejected` / `test_auth_wrong_signature_rejected` | PASS |
| 3 claim 不自报 parent_id、child 越权通用错误 | `test_claim_ignores_self_reported_parent_id` / `test_claim_child_not_in_parent_scope_generic_error` / `test_claim_invalid_pairing_code_generic_error` / `test_claim_without_parent_session_unauthorized` | PASS |
| 4 token device/child 绑定对任务与逐事件生效 | `test_tasks_today_enforces_token_child_binding` / `test_tasks_today_requires_token` / `test_event_child_not_bound_is_rejected_per_event` | PASS |
| 5 seq 42 成功响应丢失后同 event_id 重发 → duplicate+ACK | `test_duplicate_after_lost_response` | PASS |
| 6 同 event_id 不同载荷被拒且不重复落账 | `test_conflict_same_event_id_different_payload` / `test_conflict_same_event_id_different_sequence` | PASS |
| 7 gap/回退/批内失败不越过连续 ACK | `test_gap_does_not_advance_ack` / `test_sequence_regression_rejected` / `test_in_batch_failure_does_not_advance_past_failure` / `test_batch_conflict_does_not_advance_past_conflict` | PASS |
| 8 401/403 不删除 pending；业务 4xx 逐事件可修复拒绝 | `test_invalid_token_returns_401_and_state_unchanged` / `test_token_impersonating_other_device_rejected` | PASS |

**过程中修复**：① challenge 端点 `device_id` 误作 query 参数 → 改 Pydantic body（`ChallengeRequest`）；② 过期测试的 monkeypatch 破坏 `store.time` 模块引用 → 改用 `create_challenge(ttl=-10)` 直接构造过期 challenge；③ 回退测试断言修正（回退事件本身 rejected 且不推进 ACK，后续合法新事件仍可推进）。

### 5.4 CP2 范围偏差与门禁

- 范围偏差：无。
- 硬件/串口/Flash 操作：0。
- 真实凭据/儿童数据/外部服务：0（seed 均为 mock 家长/儿童/任务，无真实账号）。
- 未连真实数据库/NAS/公网；测试与手工服务仅绑定 127.0.0.1。
- 未开发 Voice/Camera/AI/4G/GPS/OTA。

### 5.5 CP2 结论

CP2 完成并推送，工作流 WB-STREAM-001 收口为 `STREAM_CHECKPOINT_READY`。CP2 精确 hash 见最终回执（本提交自身无法自引用）。停止扩项，通知 Codex 异步复检。

## 6. 建议 Codex 复检重点（CP2）

1. `/events/batch` 逐事件处理顺序（幂等先于 sequence）是否与 ARCHITECTURE.md §5.4/CR-WB002-06 完全一致；`duplicate` 是否返回当前连续 ACK。
2. 两阶段 challenge（§6.3/§6.4.3）的过期/单次/跨设备拒绝与日志脱敏是否可接受。
3. claim 家长信任链（§6.4.2）：认证上下文派生 parent_id、child 授权范围、通用外部错误（不泄露儿童存在）。
4. 内存 Mock 的并发模型（单进程串行）是否符合 MVP 预期；V1 真实后端时哪些逻辑需迁移。
5. 依赖锁定（requirements.txt）与 `.venv`/缓存未提交的边界是否满足要求。
