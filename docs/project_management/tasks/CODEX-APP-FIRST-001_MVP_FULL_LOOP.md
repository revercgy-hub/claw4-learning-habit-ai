# CODEX-APP-FIRST-001 — App-first MVP 完整链路批次

> 状态：`IN_PROGRESS`（AF0~AF2c、AF3a `CHECKPOINT_READY`；AF3b/AF4 下一步）
>
> 执行：Codex；WorkBuddy 暂停调度
>
> 用户角色：仅在阶段末按固定验收单操作真机并回传屏侧结果
>
> 目标：先在 App/Host 模式完成 L2/L3 MVP 主链路，再生成一个真机候选并只做一次阶段验收。

## 1. 为什么这样更快

后续不再使用“小改动→刷机→长时间 monitor→再修改”的逐功能节奏。速度优化来自：

1. **真实业务核心、平台边界替身**：App 模式复用真实 `LearningApp`、Domain、Coordinator、Outbox、Backend 与 PWA，只替换 NVS/网络/时钟/设备 UI 等硬件边界。
2. **一条自动验收命令**：把 C++、Backend、PWA、跨语言 E2E、故障注入与 IDF build 汇总为批次 gate；失败留结构化报告，不依赖人工抄串口。
3. **分层测试频率**：每次提交跑受影响的快速测试；checkpoint 跑完整 App gate；阶段末才跑 IDF 全量 build 和真机。
4. **屏上可观测性**：候选固件显示 build ID、网络/认证状态、pending/ACK、最近错误码和重试状态。用户提供结果页或照片即可，默认不连接 monitor。
5. **一次候选、一次验收**：阶段冻结后生成唯一 SHA-256 候选；真机失败时先凭诊断码定位，仅证据不足才抓串口。

### 1.1 已确认的当前瓶颈

- 现有 `run-host-mvp-e2e.ps1` 只是依次运行 C++/pytest/PWA 三套 gate；Python E2E 仍手工构造设备事件，不是“真实 C++ LearningApp → HTTP → Backend → PWA”的单链。
- 11 个 C++ 测试目标会重复编译同一批实现源码；缺少 CMake/Ninja object library、CTest filter 与并行执行。
- ready 工作树没有可复用的 backend venv/node_modules bootstrap；冷环境重复安装浪费时间。
- repo → `E:/c` 目前依赖手工复制，缺少 manifest/hash 一致性检查；容易出现源码已改但 IDF 镜像陈旧。
- 当前没有 desktop Learning App、LVGL SDL target 或 Playwright/Cypress。所谓 App 模式目前只有 headless `LearningApp` + fakes，必须先补 connected runner；不能把不存在的模拟器写成已具备。

因此 AF0 优先投入“减少每轮固定成本”，AF1/AF2 再补真实纵向链；LVGL SDL 只有在 UI 迭代成为实际瓶颈后才进入，不作为本批前置门槛。

## 2. “完整链路”边界

本批完整链路定义为 L2/L3 的 MVP 主链路：

```text
家长 PWA 创建今日任务
→ Backend 持久化
→ 设备侧 App 认证并拉取任务
→ Start / Pause / Resume / Complete
→ 断网期间本地持久化与重启恢复
→ 网络恢复后批量同步与 ACK 收敛
→ PWA 恰好一次显示学习记录和完成状态
```

本批不包含 L4 Voice、L5 MCP 真机注册、L6 AI Coach、摄像头、4G/GPS 和发布 OTA。它们分别进入后续批次，避免音频/外部服务/隐私变量阻塞核心闭环。

## 3. App/Host 模式定义

App/Host 模式不是脚本伪造整个设备。它必须使用：

- 真实 C++ `LearningApp` / reducer / coordinator / outbox / presenter；
- 可重启的文件或内存持久化适配器；
- 真实 Backend HTTP 契约和数据库；
- 真实 PWA 构建与组件/浏览器验收；
- 平台边界上的确定性 fake：时钟、链路中断、设备调度与屏幕输出；
- 生产设备薄适配器仍进入 `idf.py build`，但阶段结束前不 app-flash。

任何仅由 Python 合成设备直接写 Backend 的测试，不能单独作为本批完整链路通过证据。

## 4. Checkpoint

### AF0 — 基线冻结与统一 Gate

- 基线：L1c `DEVICE_L1C_PERSISTENCE=PASS` 对应代码 `4db2283`。
- 新增可复用依赖 bootstrap（venv/npm cache）和统一 `Quick/Full/IdfBuild` 验证入口，输出机器可读 summary（命令、提交、通过数、耗时、失败阶段）。
- 将 host C++ gate 迁到 CMake/Ninja object library + CTest，避免 11 个目标重复编译实现源码，并支持按测试名过滤和并行执行；迁移期间保留旧脚本作结果交叉校验。
- 复跑现有 C++ 11 个测试二进制、接口 cross-check、Backend、PWA 和现有 E2E。
- 新增 Virtual Device App runner：直接组装真实 `LearningApp` + 同一 outbox codec + 可重启文件存储，通过 JSONL `boot/restart/touch/network/sync` 命令驱动；状态输出只能来自真实 DomainState/presenter，禁止在 runner 复制业务状态机。

验收：进程 kill/restart 可模拟掉电恢复；冷启动可复现；无工作区生成物入 Git；失败能定位到具体层。

### AF1 — 真实 C++ App ↔ Backend 契约链

#### AF1a — target-portable wire codec（已完成）

- 提交 `ea3afb2`：Auth challenge/auth、Today、Events/ACK 的 C++17 JSON codec 与负例测试。
- host unit `12/12`、interface cross-check `34/34 + 3/3 + 1/1` PASS。
- 证据：`docs/project_management/reports/CODEX_APP_FIRST_AF1A_2026-09-05.md`。

#### AF1b — transport boundary（已完成）

- 提交 `9ecd247`：`BackendClient` + 可注入 `HttpTransport`，锁定 challenge/auth/today/events batch 顺序、Bearer header、ACK/error 分类。
- host unit `13/13`、interface cross-check PASS；证据 `CODEX_APP_FIRST_AF1B_2026-09-05.md`。

#### AF1c — transport relay（下一步）

- 已完成 wire contract loop：C++ fixture 生成 challenge/auth/events JSON，真实 Backend 返回 Today/ACK 后由同一 C++ decoder 解析；accepted→duplicate 幂等证据通过。
- 证据：`reports/CODEX_APP_FIRST_AF1C_2026-09-05.md`；下一阶段 AF2 把真实 coordinator/outbox 接到网络端口并进入故障矩阵。

- 实现设备侧 Backend/Sync 的平台无关客户端核心；测试中连接真实本地 Backend，而不是 fake sink。
- 覆盖设备注册/认证、今日任务、事件 batch、ACK 连续前缀、duplicate 幂等和 auth pause/reset。
- Auth/Today/Events/ACK 的 JSON codec 必须是 target-portable C++，由 runner 与设备共用；Python 只允许作 HTTP relay，不得手写或改写 C++ 事件。
- Backend schema 兼容由契约测试锁定。

验收：PWA 建任务后，真实 C++ App 能拉取并完成任务，Backend/PWA 只出现一次完成事实。

### AF2 — Offline / Restart / Fault Matrix

#### AF2a — Virtual Device 故障子集（已完成）

- JSONL runner 已支持 offline、lost response、duplicate ACK、restart；真实 coordinator/outbox 验证 pending 保持、活动会话恢复和最终收敛。
- 证据：`reports/CODEX_APP_FIRST_AF2A_2026-09-05.md`。

#### AF2b — PWA exactly-once 与重复矩阵（已完成）

- C++ fixture 生成三条业务事件，真实 Backend/PWA 投影完成；相同 event_id/sequence 重复 10 轮，3 accepted + 27 duplicate，PWA 只见 1 条 completed session。
- 证据：`reports/CODEX_APP_FIRST_AF2B_2026-09-05.md`。

#### AF2c — 认证/存储/ACK 缺口与重启矩阵（已完成）

- 收口 auth pause、storage commit failure、ACK gap、连续重启和 BackendClient→coordinator 桥接；host unit `14/14` PASS。
- 证据：`reports/CODEX_APP_FIRST_AF2C_2026-09-05.md`。

- 场景：启动前断网、完成时断网、请求已到但响应丢失、连续重启、旧 pending、ACK 缺口、认证失效、存储提交失败。
- 所有状态变更继续遵守 commit-then-publish；未持久化不得更新 UI 成功态。
- 浏览器级 PWA E2E 驱动真实页面创建任务与核对记录；不得只用 Python 直接调用 API 代替用户链路。
- 至少 10 轮重复执行，无事件丢失、无重复统计、sequence 单调。

验收：恢复网络后 pending 收敛；PWA 与设备投影一致；任何失败都有稳定错误码。

### AF3 — Device 薄适配与诊断页（BUILD ONLY）

#### AF3a — 基线 BUILD ONLY（已完成）

- 现有 `E:\c` 镜像重复构建 PASS，binary SHA 与已验收 L1 基线一致；ota_1 既有溢出保持不触碰。
- 证据：`reports/CODEX_APP_FIRST_AF3A_2026-09-05.md`。

#### AF3b — App-first manifest/诊断候选（下一步）

- 对候选镜像登记 repo→镜像文件哈希、build ID、pending/ACK/error 诊断字段，完成后才进入 AF4 冻结。

- 接 Metalio 网络调度/传输的薄适配器，不在业务核心包含 IDF/Metalio 头。
- Learning 页面增加可折叠诊断信息：build ID、auth/network、pending、last ACK、最近错误码、最后同步时间。
- 自动化 repo → IDF 构建镜像的精确同步和 manifest/SHA 校验，消除手工 copy；只同步任务包允许路径。
- 保持懒加载和统一 Interaction 漏斗；不改 BSP、driver、sdkconfig、partition、bootloader、ota_1、C5 或 eFuse。

验收：`idf.py build` 通过；sdkconfig/partition diff=0；`ota_0` 容量通过；静态检查证明设备网络回调经主循环调度。

### AF4 — App-first 阶段验收

- 从干净环境启动 Backend/PWA/App runner，执行本文件 §2 的在线→离线→重启→恢复→同步全链。
- App E2E 连续 5 轮、进程重启 50 次、PWA browser E2E 连续 3 轮；全套 gate 通过后做一次 cold IDF build。
- 输出唯一验收报告、固件大小与 SHA-256；当前 ota_0 余量约 261,328 B，若降到 128 KiB 以下立即停止功能增长并先做尺寸治理。
- 冻结候选后不再混入新功能，只允许修复阻塞验收的问题。

验收标志：`APP_FIRST_MVP_LOOP=PASS`。未达到该标志不得安排真机。

### AF5 — 用户主导的一次性真机验收

Codex 提供不超过 15 分钟的屏侧验收单和唯一候选。用户完成：

1. 进入 Learning，确认 build ID；
2. 在线拉取家长端任务；
3. 断网后 Start→Pause→Resume→Complete；
4. 重启设备，确认完成态/待同步数仍在；
5. 恢复网络，等待 pending 归零、ACK 前进；
6. 刷新 PWA，确认学习记录恰好一次；
7. 回传 PASS/失败步骤、屏上错误码和必要照片。

默认不要求串口日志。只有失败无法由屏上状态、Backend 记录和 PWA 结果定位时，才进入一次受控 monitor 取证。

## 5. 自动门禁与停止条件

批次门禁：

- C++ 单元/契约/App 集成测试全绿；
- Backend 全测试全绿；
- PWA typecheck/lint/test/build 全绿；
- 真实 C++ App ↔ Backend ↔ PWA E2E 全绿；
- offline/restart/fault matrix 全绿；
- IDF build、镜像尺寸、sdkconfig/partition 零漂移；
- Git diff 无构建产物、日志、密钥和原始 NVS dump。

立即停止并报告：

- 数据丢失、跨家庭越权、认证绕过、事件不可幂等；
- 需要改变 API/产品权威语义；
- 需要真实账号、证书、付费服务或上传儿童数据；
- 镜像不再适配 ota_0；
- 需要触碰永久禁区；
- App gate 无法复现而必须依赖真机才能继续。

## 6. 后续批次

- `APP-FIRST-002`：L4/L5，使用录制文本/STT fixture 和 MCP JSON 先完成确定性命令与确认门禁；阶段末一次音频/语音真机验收。
- `APP-FIRST-003`：L6，先用 mock AI provider 完成建议/讲解/复盘/家长摘要；真实外部 AI、儿童数据与凭据必须另行授权。

## 7. Git 交付规则

- 开发分支：`codex/app-first-mvp-loop`（已从 AF0 `24ccf1a` 建立并推送）。
- 每个 checkpoint 独立提交并普通 push；报告记录不可变 SHA。
- Codex 直接实施与修复；本批不向 WorkBuddy 派发任务。
- 阶段末只交付一个真机候选及其 SHA-256，避免多个固件并行造成证据混淆。
