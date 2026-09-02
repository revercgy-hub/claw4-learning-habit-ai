# WB-STREAM-002：主机侧 MVP 闭环连续开发流

## 1. 调度信息

- 负责人：WorkBuddy
- 异步复检与普通缺陷修复：Codex
- 状态：`READY`
- 分支：`workbuddy/domain-offline-stream`
- 固定起始基线：以本次预检完善提交为准；精确 hash 由 Codex 调度回执给出，并须等于首次开始时的远端工作流分支
- 顺序：CP0 → CP1 → CP2 → CP3 → CP4 → CP5 → CP6 → CP7 → CP8；每个 checkpoint 验证、独立提交并普通 push 后直接继续，不等待 Codex 中途验收
- 报告：`docs/project_management/reports/WB-STREAM-002_REPORT.md`

本流连续完成可在 Windows 主机安全验证的 MVP：C++17 领域/离线/协调层、纯 UI presenter、持久化家庭后端、家长 PWA 和主机端到端闭环。不接入真机、官方固件或真实家庭数据。

截至 Codex 预检时，远端工作流分支还没有任何 WorkBuddy checkpoint；必须从 CP0 开始，不能把任务包已存在误当成任务已实施。

## 2. 开始前必须读取

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/CONTINUOUS_DEVELOPMENT.md`
4. `docs/project_management/TASK_BOARD.md`
5. `docs/project_management/WORKBUDDY_GIT_SYNC.md`
6. `docs/project_management/reports/CODEX_REVIEW_WB-STREAM-001_2026-09-02.md`
7. `docs/project_management/reports/CODEX_PREFLIGHT_WB-STREAM-002_2026-09-02.md`
8. `docs/ARCHITECTURE.md`
9. 本任务包

## 3. 全流禁止事项

- 不修改 `vendor/MetalioClaw4/**`、官方 sdkconfig、partition CSV、Bootloader、OTA 或 BSP。
- 不打开串口，不读取/擦除/刷写 Flash，不构建设备发布固件，不运行任何设备烧录命令。
- 不加入 LVGL、Wi-Fi、GPIO、ESP-IDF、FreeRTOS、NVS 实现或任何硬件依赖。
- 不连接外部/生产数据库、NAS、AI Provider、真实账号、密钥、儿童数据、付费或公网业务服务；CP5 允许临时本地 SQLite 测试库和 PostgreSQL/Compose 配置文件，但不得连接真实实例。
- 不实现 Camera、Voice、情绪/人脸识别、本地大模型、4G/GPS 或非 MVP 功能。
- 不修改任务看板、任务包、Codex 报告、同步指令或 `AGENTS.md`。
- 不 force push、rebase、reset、删除/改写分支或修改远端 URL。

## 4. CP0：建立本机 C++17“编译、链接、运行”门槛

### 目标

建立可复现的 Windows 主机测试入口。P4 交叉编译器只能继续用于接口语法检查，不能替代本机可执行单元测试。

### 允许修改

- `tools/dev/verify-host-cpp-tests.ps1`
- `firmware/tests/host/**`
- `.gitignore`（仅在现有规则确实未覆盖本地工具/输出时最小修改）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 工具链规则

1. 先搜索已有 `clang++.exe`、`g++.exe` 或 Visual Studio `cl.exe`，记录绝对路径和版本。
2. 若不存在，允许使用 `winget` 安装官方 `LLVM.LLVM` 的当前稳定版到用户范围；不得请求管理员权限，不得关闭证书校验。记录包 ID、版本、来源和实际路径。
3. Codex 于 2026-09-02 预检到 `LLVM.LLVM` 22.1.8、官方发布 URL `https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.8/LLVM-22.1.8-win64.exe`、SHA256 `16e5709785fef73c854646241c4a92c5cd574318d1b33c63330dd7721903e55c`。WorkBuddy 必须重新运行 `winget show --id LLVM.LLVM --exact --accept-source-agreements`；若版本变化，以当次 manifest 为准并在执行前后校验下载文件 SHA256，不得盲信本文旧值。
4. 优先使用 `winget install --id LLVM.LLVM --exact --scope user --accept-package-agreements --accept-source-agreements`。若 manifest 不支持 user scope，可下载经 SHA256 验证的同一官方 Nullsoft installer，静默安装到已忽略的 `E:\workbuddy\toolchains\llvm-<version>`；不得安装到仓库、请求提权或更改系统安全策略。
5. 安装包、编译器、缓存和二进制只能留在用户工具目录或已忽略的 `toolchains/`、`out/`，不得提交。
6. 若安装需要提权、来源/哈希不可验证或本机程序仍不能运行，CP0 标记 `BLOCKED` 并停止整个流；不得用交叉编译“假装”运行测试。

### 脚本要求

- 支持 `-CompilerPath`；未传入时可探测 PATH/标准安装路径，但不能写死当前用户路径。
- 以 C++17、警告视为错误编译并链接 `firmware/tests/host/smoke_test.cpp`，输出到 `out/host-tests/`，随后实际运行程序并透传退出码。
- 路径含空格和中文时必须正常工作；失败应显示编译器、命令阶段和退出码，不吞掉原始诊断。
- 继续调用现有 `verify-interface-contracts.ps1`，证明接口交叉编译未回退。

### 验收与提交

- 主机 smoke 程序真实执行并返回 0；报告必须区分 compile/link/run。
- 接口契约脚本 PASS；`git diff --check` PASS。
- 提交：`test(WB-STREAM-002): add native C++ test gate`
- push 后记录 `CP0_CHECKPOINT=<hash>`，立即进入 CP1。

## 5. CP1：纯领域 Reducer 与状态机

### 目标

实现无硬件依赖、无内部 I/O 的确定性领域 reducer：输入不可变 `DomainState`、intent 和显式上下文，输出“下一状态 + 事件”，不直接发布 UI、不写磁盘、不发网络。提交由 CP2 outbox 决定，持久化失败时旧状态仍是唯一已提交状态。

### 允许修改

- `firmware/main/learning_domain/**`
- `firmware/tests/unit/domain/**`
- `tools/dev/verify-host-cpp-tests.ps1`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现

1. Task 转换：Ready → InProgress → Paused ↔ InProgress → Completed；Skip 只在允许状态发生。
2. StudySession：Created/Running/Paused/Completed/Aborted；一个 Task 可有多个 session。
3. `completion_type` 在 Created/Running/Paused 时为空；Completed/Aborted 时必须存在且与状态一致。
4. Start/Pause/Resume/Complete/Skip 生成有序 `EventDraft`；event/session ID、epoch 时间和单调时间由调用上下文注入，禁止在 reducer 内访问系统时钟或随机数。草稿不得分配持久化 sequence；sequence 只由 CP2 outbox 的原子事务分配。
5. Timer 到 0 只能令当前专注段暂停/待用户决策，不能自动完成 session 或 Task。
6. 显式 Complete 才能生成 `task.completed` 草稿；重复 Complete 不生成新草稿，领域层也没有 sequence 分配行为。
7. 完整快照恢复同一 session；用户选择保存/结束时 session 可为 `AutoSaved`，但 Task 不自动 Completed。损坏快照产生 Aborted session，Task 仍不自动 Completed。
8. 实际专注秒数来自单调时间累计并排除暂停区间；拒绝负数和倒退时间。
9. 失败结果必须返回原始输入状态和空草稿列表；重试复用相同 event_id。由于 reducer 不拥有 sequence，失败时不存在 sequence 消耗。
10. reducer 不包含或引用 `sync`、LVGL、Wi-Fi、GPIO、ESP-IDF、FreeRTOS 或 BSP 头文件。

### 必测场景

- 所有合法转换与非法状态拒绝；Start 的 task/session 事件顺序。
- pause/resume 多轮计时；单调时钟倒退；Timer 到 0 不完成。
- 显式完成、重复完成、Skip；一个 Task 的第二个 session。
- 完整/损坏快照两条重启路径以及 Task 不被隐式完成。
- 每个失败结果的输入状态保持不变、无事件草稿；验证领域层没有 sequence 分配 API。

不得只用 `assert` 后在 Release/NDEBUG 下失效；测试 runner 要显式统计 case/assertion、失败返回非 0。目标至少 20 个独立 case，报告写出精确数量。

### 验收与提交

- 本机编译、链接并实际运行领域单测；0 失败。
- P4 接口契约仍 PASS；依赖扫描 0 个禁止头；`git diff --check` PASS。
- 提交：`feat(WB-STREAM-002): implement domain reducer`
- push 后记录 `CP1_CHECKPOINT=<hash>`，立即进入 CP2。

## 6. CP2：平台无关 Transactional Outbox 核心

### 目标

实现 `EventSink` 的平台无关事务核心和可注入存储接口，并用主机 fake 模拟掉电/重启/写失败。真实 NVS/文件系统适配器不在本流范围内。

### 允许修改

- `firmware/main/sync/**`
- `firmware/tests/unit/sync/**`
- `firmware/tests/fakes/**`
- `tools/dev/verify-host-cpp-tests.ps1`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现

1. `persistTransition` 接收完整下一领域快照与有序 `EventDraft`，在同一事务中物化全部 `DeviceEvent` 并提交；任一步失败时快照、事件和计数器都不可见。
2. Outbox 是每设备 sequence 的唯一分配者：从持久化计数器为同一转换的草稿连续赋值并原子更新计数器；失败不消耗 sequence，重启后从已提交计数继续。禁止 reducer、UI 或网络层私自分配已提交 sequence。
3. pending 上限 200；关键业务事件（至少 Task/Session Started/Completed）不能被容量策略静默丢弃。
4. `sync.failed`/`sync.recovered` 只进入受限、可合并的本地诊断槽，不进入业务 pending 队列、不占用 200 条容量。
5. ACK 清理必须同时满足逐事件结果为 Accepted/Duplicate 且 `sequence <= last_acked_sequence`；Conflict/Rejected/Gap 及 ACK gap 之后的事件全部保留。
6. 401/403/Auth、网络和 5xx 不删除 pending；业务 4xx 可标记死信摘要但保留原 event_id 和原文供修复重放。
7. fake 存储支持确定性故障注入和重新实例化，以模拟“提交前失败、提交中失败、响应丢失、进程重启”。不得将 fake 宣称为真实掉电安全或 NVS 已验证。
8. 所有 API 明确所有权和线程假设；本轮至少保证单线程确定性，不虚构真机并发安全。

### 必测场景

- 快照、多个草稿、物化事件和 sequence 计数器成功原子提交；每个故障注入点均完全回滚且不留下 sequence 空洞。
- 重启后恢复 pending、ACK 和下一 sequence；重复 replay 收敛。
- 200 条边界、非关键诊断合并、关键事件无空间时整次转换失败且旧状态保留。
- Accepted/Duplicate 清理、gap 前缀、Conflict/Rejected/Gap/Auth/Network/5xx/业务 4xx 保留策略。
- 同 event_id 修复重放不生成新 ID；非法/非连续 sequence 被拒绝且不污染存储。

目标至少 20 个独立 sync case；所有 host 测试总计与失败数写入报告。

### 验收与提交

- 主机全量测试编译、链接、运行 0 失败，连续运行 5 次稳定通过。
- P4 接口契约 PASS；禁止依赖扫描 PASS；`git diff --check` PASS。
- 提交：`feat(WB-STREAM-002): add transactional outbox core`
- push 后记录 `CP2_CHECKPOINT=<hash>`，立即进入 CP3。

## 7. CP3：设备应用协调器、同步策略与离线任务缓存

### 目标

在纯主机环境把 reducer、transactional outbox 与现有 `SyncClient` 边界串成应用协调器；仍不实现真实 HTTP/TLS、NVS 或 BSP。

### 允许修改

- `firmware/main/application/**`
- `firmware/main/sync/**`
- `firmware/tests/unit/application/**`
- `firmware/tests/unit/sync/**`
- `firmware/tests/fakes/**`
- `tools/dev/verify-host-cpp-tests.ps1`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现与验证

1. Intent → reducer draft → outbox commit → 只在 commit 成功后发布只读状态；失败时 UI 永远看不到未提交状态。
2. 今日任务缓存按 `(task_id, version)` 增量合并，不覆盖本地 active session；空任务是正常状态。
3. 同步批次只发送 pending 连续序列；按逐事件 Accepted/Duplicate 与连续 ACK 清理。
4. Auth 失败只触发一次可注入 re-auth，再失败则暂停同步且 pending 不动；Network/5xx 使用带抖动的可注入确定性退避，上限 60 秒。
5. Conflict/Rejected/Gap、业务 4xx、响应丢失与重复响应均按架构保持/收敛；诊断事件不递归进入业务队列。
6. 所有时间、随机抖动、transport 和凭据刷新均通过接口注入；测试不得联网或 sleep。

至少 18 个独立 case，覆盖在线、离线、空任务、响应丢失、401/403、DNS/5xx、gap/conflict、缓存版本和 outbox 写失败。主机全量 C++ 测试及接口交叉编译均须 PASS。

### 提交

- `feat(WB-STREAM-002): integrate host application coordinator`
- push 后记录 `CP3_CHECKPOINT=<hash>`，立即进入 CP4。

## 8. CP4：Home/Focus/Done/Offline 纯 UI Presenter

### 目标

完成与 LVGL 解耦的四页 presenter/view-model 和 intent 映射，使设备页面逻辑可在主机测试。该 checkpoint **不等于真机 LVGL 页面已实现或验证**。

### 允许修改

- `firmware/main/ui/**`
- `firmware/tests/unit/ui/**`
- `firmware/tests/fakes/**`
- `tools/dev/verify-host-cpp-tests.ps1`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现与验证

1. Home：今日任务卡、无任务、pending sync、offline 标识；只发 `StartTask` intent。
2. Focus：任务名、剩余时间、Running/Paused、Pause/Resume/Complete；倒计时到 0 只提示决策，不自动完成。
3. Done：实际学习时长和简单 `+XP` 展示，不引入商城、排行或复杂动画。
4. Offline：缓存任务仍可进入学习，清晰显示待同步数量和可恢复错误。
5. 快速重复点击、重复 Complete、页面快速切换、状态更新乱序不得绕过领域状态机或产生重复 intent。
6. Presenter 不包含 LVGL、网络、GPIO、ESP-IDF 或 BSP；样式只输出可测试语义模型。

至少 18 个独立 UI case；主机全量 C++ 测试及禁止依赖扫描 PASS。

### 提交

- `feat(WB-STREAM-002): add host UI presenters`
- push 后记录 `CP4_CHECKPOINT=<hash>`，立即进入 CP5。

## 9. CP5：持久化家庭后端与家长 API

### 目标

把内存 Mock 演进为可测试的家庭后端应用层，同时保留原契约测试。使用 SQLAlchemy 仓储抽象：测试默认 SQLite，部署配置面向 PostgreSQL；不把数据库方言写入业务服务。

### 允许修改

- `backend/**`
- `deploy/**`
- `docker-compose.yml`
- `.env.example`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现

1. 持久化模型/迁移：parents/users 最小身份、children、parent_child、devices、tasks、study_sessions、events、device_configs；rewards 仅保留 MVP `+XP` 字段，不扩展体系。
2. 保留设备 register/challenge/claim/auth/today tasks/events 契约和全部旧回归测试。
3. 新增家长侧最小 API：创建/更新今日任务、Dashboard 汇总、学习记录、设备列表；所有 child/device 查询逐请求校验当前家长归属。
4. `/events/batch` 在同一数据库事务中完成幂等落库、连续 ACK 和 Task/StudySession read-model 投影；响应丢失重发不得重复统计。
5. Heartbeat/config 只实现 MVP 字段；未知电量必须为 null/unknown，不能伪造真机值。
6. 时间/日期使用可注入时钟和明确时区，不得继续硬编码 `2026-09-02`。
7. 提供无密钥 `.env.example` 和 PostgreSQL/backend 的 Compose 配置；Docker 当前主机未检出，允许只做配置解析/静态验证，报告不得宣称容器已运行。
8. 家长身份使用明确标注的单家庭开发会话桩；不得伪装成生产级登录，不把 token/secret 写日志。

### 必测场景

权限隔离、事务回滚、并发同 sequence、重复/冲突/gap、解绑旧 token、任务 CRUD、事件投影、Dashboard/学习记录、空任务、时区边界、后端重启后 ACK/幂等保持。旧测试必须全部保留；新增至少 30 个 backend case，`pip check` PASS。

### 提交

- `feat(WB-STREAM-002): add persistent family backend`
- push 后记录 `CP5_CHECKPOINT=<hash>`，立即进入 CP6。

## 10. CP6：家长 Responsive PWA

### 目标

使用 React + TypeScript + Vite 建立可构建的 PWA，只覆盖 Dashboard、今日任务、学习记录、设备四页，并经后端 API client 访问家庭后端。

### 允许修改

- `frontend/**`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现与验证

1. Dashboard：计划数、完成数、完成率、专注分钟、当前学习状态。
2. 今日任务：列表与创建/编辑表单，字段仅科目、任务、预计分钟、优先级；完整 loading/empty/error 状态。
3. 学习记录：时间、任务、实际时长、暂停、完成状态。
4. 设备：在线状态、电量 unknown、固件版本、最后同步时间。
5. API base URL 来自环境配置；不得直连设备、写死 token、嵌入真实数据或调用 AI Provider。
6. PWA manifest/service worker 只缓存静态壳；不得缓存认证响应、儿童 API JSON 或秘密。
7. 响应式覆盖常见手机与桌面宽度；键盘操作、label、焦点和基本对比度可测试。
8. 使用项目 `package-lock.json` 锁定依赖；不引入重量级图表、Native App 或无关 UI 框架。

当前主机已预检 Node 24.14.1/npm 11.11.0。要求 TypeScript 检查、lint、单元/组件测试（至少 20 case）、production build 全部 PASS；`npm audit --omit=dev` 的 high/critical 必须为 0，否则修复或 `BLOCKED`。

### 提交

- `feat(WB-STREAM-002): add parent PWA`
- push 后记录 `CP6_CHECKPOINT=<hash>`，立即进入 CP7。

## 11. CP7：主机端 MVP 闭环集成验证

### 目标

用合成家庭/儿童数据在 127.0.0.1 或 FastAPI in-process client 完成端到端契约，不接真机、不开放公网监听。

### 允许修改

- `backend/tests/e2e/**`
- `backend/tests/fixtures/**`
- `frontend/**`（仅集成测试与必要修复）
- `firmware/tests/**`（仅共享契约 fixture/host 集成测试与必要修复）
- `tools/dev/run-host-mvp-e2e.ps1`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须跑通

1. 家长创建今日任务。
2. 合成设备 register → claim → challenge/auth → 拉取任务。
3. 主机领域/outbox 测试证明 Start/Pause/Resume/Complete 在离线期间进入 pending。
4. 恢复连接后提交事件；模拟一次响应丢失后用同 event_id 重发，服务端返回 duplicate 且 ACK 收敛。
5. 家长 Dashboard 与学习记录只出现一次完成事实、正确实际时长和完成时间。
6. 第二家庭无法读取或修改第一家庭 child/device/task/session。
7. PWA production build 使用同一 API schema；接口漂移必须由自动测试失败暴露。

脚本必须可重复执行、自动清理临时进程/数据库，仅绑定 `127.0.0.1`，失败透传非 0。不得把 Python 模拟器结果写成 C++ HTTP、TLS、真机或浏览器 E2E 已验证。

### 提交

- `test(WB-STREAM-002): verify host MVP loop`
- push 后记录 `CP7_CHECKPOINT=<hash>`，立即进入 CP8。

## 12. CP8：全流稳定性、证据与交付收口

### 目标

统一复跑并修复 CP0～CP7 的普通缺陷，形成可供 Codex 最终验收的不可变证据。可在本流既有 host-only 允许路径内修复测试失败；不得借机扩项。

### 允许修改

- CP0～CP7 已授权的全部 host-only 路径
- `tools/dev/**`
- `docs/HOST_MVP_ACCEPTANCE.md`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 验收

1. C++ 主机全量 compile/link/run 连续 5 轮 0 失败；P4 接口交叉编译 PASS。
2. Backend 全量 pytest 连续 5 轮 0 失败，`pip check` PASS。
3. PWA typecheck/lint/test/build 连续 3 轮 0 失败，production audit 无 high/critical。
4. Host MVP E2E 连续 5 轮 0 失败；所有临时服务只监听 127.0.0.1 并在结束后退出。
5. 扫描禁止硬件依赖、秘密/真实数据、构建产物、绝对用户路径、`verify=false`、公网监听和越界文件。
6. `docs/HOST_MVP_ACCEPTANCE.md` 明确区分 `HOST_VERIFIED`、`HARDWARE_VERIFY_REQUIRED` 和未实现项；不得称 MVP 真机闭环或 Release 已完成。
7. `git diff --check`、报告链接、精确 checkpoint hash、远端同步和干净工作区全部通过。

### 提交与停止

- 提交：`test(WB-STREAM-002): stabilize host MVP evidence`
- push 后给出 CP0～CP8 精确 hash 和 `STREAM_REVIEW_READY`，停止扩项，等待 Codex 一次性最终验收与修复。

## 13. 工作流报告最低内容

每个 checkpoint 更新同一报告并记录：父提交、提交主题（当前提交自身可按规范描述）、修改文件、验收逐项结果、完整命令、编译器版本、case/assertion 数、关键输出、范围偏差、未解决风险和 Codex 复检重点。下一 checkpoint 回填上一 checkpoint 的精确 hash。

## 14. 停止条件

- CP0 无法获得能在本机执行测试二进制的可信 C++17 工具链。
- 当前 checkpoint 验证失败且无法在允许路径内修复。
- 需要修改范围外文件、官方固件、真机、Flash、分区、NVS 实现或硬件配置。
- 需要管理员权限、关闭安全校验、真实凭据/儿童数据或外部业务服务。
- 领域契约存在两种不兼容解释，且架构文档无法唯一决定。
- Backend/PWA 依赖出现无法在允许范围修复的 high/critical 漏洞，或需要真实外部服务才能继续。

触发后保留已推送 checkpoint，在报告中标记 `BLOCKED` 并停止；不得跳过失败项或自行开启 LVGL/真机/设备网络适配。普通测试/实现缺陷须先在当前 checkpoint 范围内修复，不应为了等待 Codex 而中断整个预授权流。
