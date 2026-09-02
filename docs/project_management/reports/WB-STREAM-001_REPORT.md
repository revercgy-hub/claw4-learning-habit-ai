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
| CP0 架构契约收口（CR-WB002-06~10） | `CHECKPOINT_READY` | `docs(WB-002): close replay and claim contracts`（本提交自身） | JSON 12/12、链接 21/21、章节 13、标签 157、旧语义零残留、`git diff --check` PASS |
| CP1 设备侧接口骨架 | `QUEUED` | — | — |
| CP2 FastAPI Mock Backend | `QUEUED` | — | — |

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

CP0 完成并推送，进入 CP1。CP0 精确 hash 将在 CP1 报告更新时回填（本提交自身无法自引用）。

## 3. 建议 Codex 复检重点（CP0）

1. 逐事件处理顺序（§5.4）"幂等先于 sequence"是否可唯一驱动 CP2 Mock 测试（响应丢失重发 → duplicate）。
2. challenge 两阶段（§6.3/§6.4.3）请求/响应与防重放规则是否可实现、可测试。
3. claim 家长信任链（§6.4.2）——认证上下文派生 parent_id + child 授权校验 + 通用外部错误。
4. 损坏快照唯一语义（§4.3/§4.4/§4.5/§10.3）是否只剩一套可测试语义。
5. §12.1 D11/D12 决策是否与 §5.4/§6.3 自洽。
