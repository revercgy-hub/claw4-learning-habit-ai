# WB-STREAM-001：MVP 核心连续开发流

## 1. 调度信息

- 负责人：WorkBuddy
- 复检与后续修复：Codex
- 状态：`READY`
- 分支：`workbuddy/mvp-core-stream`
- 执行方式：CP0 → CP1 → CP2 连续执行；每个 checkpoint 验证、提交、push 后直接继续，不等待 Codex
- 最终报告：`docs/project_management/reports/WB-STREAM-001_REPORT.md`

## 2. 全局必读

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/CONTINUOUS_DEVELOPMENT.md`
4. `docs/project_management/TASK_BOARD.md`
5. `docs/project_management/WORKBUDDY_GIT_SYNC.md`
6. `docs/ARCHITECTURE.md`
7. 本任务包

## 3. 全局禁止事项

- 不修改 `vendor/MetalioClaw4/**`、官方 sdkconfig、partition CSV 或官方 BSP。
- 不连接串口，不读取/擦除/刷写 Flash，不构建设备发布固件。
- 不创建 LVGL 实机页面，不声称屏幕、触摸、音频、摄像头、电源或存储真机通过。
- 不接入真实数据库、NAS、AI Provider、儿童账号、真实凭据或付费服务。
- 不开发 Voice、Camera、AI、4G/GPS、OTA 或非 MVP 功能。
- 不 force push、rebase、reset、删除远端分支或修改远端 URL。

## 4. CP0：关闭 WB-002 剩余契约问题

### 目标

完成 `CODEX_REVIEW_WB-002_ROUND2_2026-09-02.md` 的 CR-WB002-06～10，为后续 Mock 与接口建立唯一契约。

### 允许修改

- `docs/ARCHITECTURE.md`
- `docs/project_management/reports/WB-002_REPORT.md`
- `docs/project_management/reports/WB-STREAM-001_REPORT.md`

### 必须完成

1. 认证和归属校验后先做 `(device_id,event_id)` 幂等查重；只对新事件执行 sequence 连续性检查。
2. 同 event_id 同内容重发返回 `duplicate`；同 ID 不同 sequence/type/payload 摘要返回冲突并告警。
3. 补齐 nonce/challenge 获取调用、签名绑定、单次使用、过期和重放规则。
4. claim 使用已认证家长上下文，移除请求体 `parent_id`，校验 child 归属。
5. 损坏快照只为 `aborted`；完整快照恢复后由用户结束/保存才可 `auto_saved`。

### 验证与提交

- 执行 Round 2 报告和 `WORKBUDDY_GIT_SYNC.md` 中的全部扫描。
- JSON 全部可解析，Markdown 本地链接全部存在，`git diff --check` 无输出。
- 提交：`docs(WB-002): close replay and claim contracts`
- push 后保存精确 hash；立即进入 CP1，并在 CP1 的报告更新中回填 `CP0_CHECKPOINT=<hash>`。

## 5. CP1：设备侧接口骨架与交叉编译契约检查

### 目标

建立独立于官方 BSP 的接口骨架，不实现硬件或复杂业务逻辑。仓库中的 `firmware/main/` 对应架构文档所称设备项目 `main/`。

### 允许修改

- `firmware/main/learning_domain/**`
- `firmware/main/sync/**`
- `firmware/main/ui/**`
- `firmware/main/assistant/**`
- `firmware/main/telemetry/**`
- `firmware/tests/contracts/**`
- `tools/dev/verify-interface-contracts.ps1`
- `docs/project_management/reports/WB-STREAM-001_REPORT.md`

### 最低接口

- `learning_domain`：Task/StudySession/DeviceEvent/DomainState 的 ID、枚举、只读快照、intent 与结果类型。
- `sync`：EventSink、SyncClient、连续 ACK/逐事件结果、错误分类接口；不得实现网络或持久化。
- `ui`：只读 ViewState 与 IntentSink；不得包含 LVGL 或直接写业务状态的方法。
- `assistant`：命令到领域 intent 的 Router 接口；不得接入 LLM/ASR/TTS。
- `telemetry`：脱敏结构化日志接口；不得记录 token、secret、儿童姓名或内容数据。

### 技术与验证约束

- 使用可移植 C++17；`learning_domain` 禁止包含 LVGL、Wi-Fi、GPIO、ESP-IDF、FreeRTOS 或官方 BSP 头文件。
- 不创建实现复杂逻辑；本 checkpoint 只定义边界和最小值类型。
- `verify-interface-contracts.ps1` 接受 `-CompilerPath` 参数，输出到忽略的 `out/`，以 P4 交叉编译器执行 `-fsyntax-only` 或等价编译期检查。
- 编译器可从 `E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe` 传入；脚本不得把此用户路径写死为唯一运行方式。
- 记录依赖扫描与交叉编译结果。当前主机没有本机 C++ 编译器，本 checkpoint 不得声称运行了 C++ 单元测试。
- 提交：`feat(WB-STREAM-001): add MVP interface contracts`
- push 后保存精确 hash；立即进入 CP2，并在 CP2 的报告更新中回填 `CP1_CHECKPOINT=<hash>`。

## 6. CP2：可执行 Mock Backend 与契约测试

### 目标

建立不依赖真实数据库或外部服务的 FastAPI 内存 Mock，使身份链路、今日任务和事件同步契约可自动测试。

### 允许修改

- `backend/**`（`.venv`、缓存、日志与测试输出不得提交）
- `docs/project_management/reports/WB-STREAM-001_REPORT.md`

### 依赖规则

- 使用项目局部 `backend/.venv`，不得污染 ESP-IDF Python 环境或系统 Python。
- 允许安装并锁定 FastAPI、Uvicorn、Pytest、HTTPX 及其必要依赖；提交声明式依赖文件和锁定结果，不提交虚拟环境。
- 不启动公网监听；测试和手工运行只绑定 `127.0.0.1`。

### 最低端点

- `POST /api/v1/devices/register`
- 架构 CP0 选定的 challenge 获取端点
- `POST /api/v1/devices/auth`
- `POST /api/v1/devices/claim`
- `GET /api/v1/children/{child_id}/tasks/today`
- `POST /api/v1/events/batch`
- `GET /health`

### 必测场景

1. register 由服务端签发 device_id；测试日志不泄露 secret。
2. challenge 过期、重复使用和跨设备复用被拒绝。
3. claim 请求体不能自报 parent_id；child 不属于已认证家长时使用通用外部错误。
4. token 的 device/child 绑定对任务查询和每个事件生效。
5. seq 42 成功但客户端假设响应丢失后，以同 event_id 重发，返回 `duplicate` 和当前连续 ACK。
6. 同 event_id 不同载荷被拒绝并不重复落账。
7. 新事件 sequence gap、回退和批内失败不越过连续 ACK。
8. 401/403 不删除 pending 业务事件；业务 4xx 返回可修复的逐事件拒绝语义。

### 验证与提交

- 执行完整测试并记录测试数量、通过/失败和命令；`git diff --check` 无输出。
- 提交：`feat(WB-STREAM-001): add contract mock backend`
- 报告中把 CP2 写为“本 checkpoint 提交自身”；push 后在最终回执给出精确 `CP2_CHECKPOINT=<hash>` 和 `STREAM_CHECKPOINT_READY`，停止扩项并通知 Codex。

## 7. 工作流停止条件

- 当前 checkpoint 验证失败且无法在允许路径内修复；
- 需要修改官方源码、真实硬件、Flash、分区或设备配置；
- 需要真实账号、密钥、儿童数据或公网服务；
- 必须修改允许路径之外的文件；
- CP0 无法形成唯一契约，导致 CP1/CP2 存在两种不兼容实现。

触发时保留已完成 checkpoint，报告 `BLOCKED`，不得跳到后续任务。
