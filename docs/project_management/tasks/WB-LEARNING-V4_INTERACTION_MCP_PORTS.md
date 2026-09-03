# WB-LEARNING-V4 阶段 2：V4 §7 Interaction Router / §8 Learning MCP Host / §9 Platform Ports（P14–P16）

## 1. 调度信息

- 负责人：WorkBuddy
- 状态：`READY`
- 分支：`workbuddy/learning-v4-host-sync`
- 固定基线：`3e72cd8`（WB-LEARNING-V4 C4，docs: 流报告 + integration map openclaw 补核 + 看板 6.17 行）
- 顺序：P14（Interaction）→ P15（Learning MCP Host）→ P16（Platform Ports）；每个 checkpoint 独立验证、独立提交并 push 后继续
- 复检：用户 2026-09-03 起取消 Codex 复检，收口证据以 WorkBuddy 报告为准，由用户最终决策
- 报告：`docs/project_management/reports/WB-LEARNING-V4_REPORT.md`

本阶段只做 V4 规划 P14–P16 中**可在 Windows 主机验证**的契约与纯逻辑：交互漏斗、STT 短语映射、MCP 工具宿主（Mock Transport=直接调用）与 8 个平台 Port 抽象及其 fake。不碰 Metalio 源码、不建真机固件、不接真实 MCP transport（Metalio `ParseMessage`/`AddTool` 属 §10/P17 设备适配，另行授权）。

## 2. 开始前必须读取

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `项目总规划/WORKBUDDY_CLAW4_学习伙伴_完整开发提示词_V4.md`（§7–§9）
4. `项目总规划/CLAW4_学习伙伴_任务规划_V4.md`（P14–P16）
5. `docs/project_management/TASK_BOARD.md`
6. `docs/ARCHITECTURE.md`（§3.5/§3.6/§4/§5/§8.1）
7. `docs/project_management/reports/WB-LEARNING-V4_REPORT.md`
8. 本任务包

## 3. 全流禁止事项

- 不修改 `vendor/MetalioClaw4/**`、官方 sdkconfig、partition CSV、Bootloader、OTA、BSP。
- 不 include LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / NVS / Metalio 头；新增目录通过 include 扫描门禁。
- 不实现 ASR/TTS/唤醒词/LLM；STT mapper 只是「固定短语 → Command」的纯查表，无任何 AI Provider。
- 不接真实 MCP transport 与真实 HTTP/WS/MQTT；不连接外部服务、真实账号、密钥、儿童数据。
- 不修改既有 `learning_domain` reducer/状态机语义、`sync`、`application` 行为（只读复用）。
- **AI/MCP 一律不允许直接 Complete**：`request_complete_task` 只产生待确认请求，必须经孩子/用户在设备上的物理确认（Touch 源）后才映射为 `Intent::Complete`。
- 不 force push、rebase、reset、删除/改写分支、修改远端 URL。
- 不把自己标为 ACCEPTED、不合并 main。

## 4. 语义决策（本阶段固化的 MVP 规则，写入代码注释与报告）

1. **统一漏斗**：Touch / Voice(STT 短语) / MCP 三类输入全部汇入 `interaction::CommandDispatcher`，域变更只从这一个出口进入（V4 §7）。
2. **完成门禁**：`CompleteTask` 仅 Touch 源直接放行；Voice/MCP 源只登记 pending 待确认，`confirmPendingComplete()`（仅 Touch 语义）才真正发出 `Intent::Complete`；重复请求不重复登记。
3. **查询不经过域变更**：`QueryTodayTasks/CurrentTask/RemainingTime/TodayProgress` 是只读投影，由 MCP Host/UI presenter 直接读 `DomainState`，dispatcher 对查询载荷不产生任何 sink 调用。
4. **reducer 以 task_id 为键**（`reducer.cpp` 入口强制 `request.task_id` 存在）：所有变更载荷必须携带 `task_id`；会话由该任务绑定的 `active_session` 推导，dispatcher/MCP 不做域外推断。
5. **get_remaining_time 复用 `ui::buildFocus` 数学**（`planned_ms - elapsed_ms`，elapsed 含当前 Running 段），不另造公式。
6. **get_today_progress**：`snapshot().tasks` 即「今日任务缓存」；`completed/(completed+inprogress+paused+ready+skipped)`，排除 `Pending`（未来日期不可调度项），denominator 为 0 时进度 0。MVP 明确规则，不等同于家长端统计口径。
7. **MCP 响应为 JSON-lite 字符串**（最小转义），真实 JSON 编解码与 transport 是 §10/P17 设备适配职责。

## 5. P14 — Interaction Router / Dispatcher（含 STT mapper）

### 目标

在 `firmware/main/interaction/` 建立输入无关的交互契约层：Command 词汇 + 载荷、纯 STT 短语映射、统一 CommandDispatcher（含完成确认门禁）。纯 C++17，无硬件头，不触碰 reducer。

### 允许修改

- 新增 `firmware/main/interaction/**`（command.h、stt_mapper.h/.cpp、dispatcher.h/.cpp）
- 新增 `firmware/tests/fakes/fake_command_sink.h`、`firmware/tests/unit/interaction/dispatcher_tests.cpp`
- `tools/dev/verify-host-cpp-tests.ps1`（implRoots 增 `interaction` 目录；其余改动见 P16 门禁段）
- **P14.1 既有 presenter 集成修正（有界）**：`firmware/main/ui/presenters.cpp` 的 `mapFocusTap` 目前只填 `session_id` 不填 `task_id`，而 reducer 强制以 `task_id` 为键（`reducer.cpp` 入口），直连 dispatcher/coordinator 会恒 `TaskNotFound`。补 `r.task_id = v.task_id;`（FocusView 已携带），并在 `firmware/tests/unit/ui/presenter_tests.cpp` 对既有两处 mapFocusTap case 增加 task_id 断言。不得改动其他 presenter 行为。

### 必须实现

1. `interaction/command.h`：`CommandKind`（StartTask/PauseTask/ResumeTask/CompleteTask/SkipTask + 4 个 Query 类 + Unknown）、`CommandPayload`（kind + optional task_id/session_id）。既有 `assistant/command.h` 保持不变（voice 层自有词汇），本层为输入无关规范词汇。
2. `interaction/stt_mapper.h/.cpp`：`SttMapping mapVoicePhrase(const std::string&)` 纯查表。规范化=trim+ASCII 小写；先精确全短语表，后按「动作词优先、查询词其次」的 contains 回退。MVP 短语表写入 .cpp 注释（开始/暂停/继续/完成/跳过/任务/在学/多久/进度等）。
3. `interaction/dispatcher.h/.cpp`：
   - `CommandSink`（`emit(IntentRequest) -> IntentResult`，host 下由 AppCoordinator 胶水/测试 fake 实现，设备下由 P17 adapter 实现）；
   - `DispatchStatus`（Emitted / NeedsUserConfirmation / NoPendingConfirmation / UnsupportedKind）与 `DispatchResult{status, intent_result}`；
   - `CommandDispatcher(sink)`：`dispatch(CommandSource, CommandPayload)`、`confirmPendingComplete()`、`cancelPendingComplete()`、`hasPendingComplete()`；内部纯映射 `CommandKind -> IntentRequest`（携带 task_id/session_id pass-through）。

### 必测场景（dispatcher_tests.cpp，显式 case 计数）

1. Touch StartTask 携带 task_id → sink 收到 `Intent::StartTask` 且 id 一致，status=Emitted。
2. Touch Pause/Resume/Skip 映射正确。
3. Touch CompleteTask → 直接 emit `Intent::Complete`（不进入 pending）。
4. Voice/MCP CompleteTask → status=NeedsUserConfirmation，sink **零调用**，pending 登记。
5. 重复 Voice/MCP 完成请求 → 仍只 1 个 pending，不重复 emit。
6. `confirmPendingComplete()` → 真正 emit 一次 Complete，pending 清空。
7. 无 pending 时 `confirmPendingComplete()` → NoPendingConfirmation，sink 零调用。
8. `cancelPendingComplete()` 后 confirm → NoPendingConfirmation。
9. 4 个 Query 载荷 → sink 零调用（UnsupportedKind 或等价，不产生域变更）。
10. STT mapper：`开始学习`→StartTask、`完成任务`→CompleteTask（不被「任务」查询词劫持）、`今天进度`→QueryTodayProgress、`随便聊聊`→Unknown/未识别。

### 验收与提交

- host gate（见 P16 §8）compile/link/run PASS，新测试 exit 0；`git diff --check` PASS。
- 提交：`feat(WB-LEARNING-V4): interaction dispatcher + stt mapper (P14)`；push 后记录 hash。

## 6. P15 — Learning MCP Host（8 个 learning.* 工具）

### 目标

在 `firmware/main/mcp/` 提供学习域 MCP 工具宿主：8 个 `learning.*` 工具签名与纯逻辑，变更工具**经 P14 CommandDispatcher（Mcp 源）**进入域，查询工具直接读后端快照。Mock Transport=host 测试直接调 `invoke()`。

### 允许修改

- 新增 `firmware/main/mcp/learning_mcp_host.h/.cpp`
- 新增 `firmware/tests/fakes/fake_learning_backend.h`、`firmware/tests/unit/mcp/learning_mcp_host_tests.cpp`
- `tools/dev/verify-host-cpp-tests.ps1`（implRoots 增 `mcp`）

### 必须实现

1. `LearningBackend`（注入边界）：`DomainState snapshot() const`、`int64_t nowMonotonicMs() const`（host 下由胶水/测试 fake 实现；变更一律走 dispatcher，不在此接口上开 emit 后门）。
2. `LearningMcpHost(CommandDispatcher&, LearningBackend&)`：
   - 工具名常量 `learning.get_today_tasks` / `learning.get_current_task` / `learning.get_remaining_time` / `learning.get_today_progress` / `learning.start_task` / `learning.pause_task` / `learning.resume_task` / `learning.request_complete_task`；
   - `McpRequest{tool, args(map<string,string>)}`、`McpResponse{ok, payload, error}`；
   - 查询：get_today_tasks（任务列表 JSON-lite）、get_current_task（active_session 对应任务 + 会话状态）、get_remaining_time（复用 buildFocus 数学；无会话返回 active=false）、get_today_progress（§4.6 规则）；
   - 变更：start/pause/resume 解析 args.task_id → dispatch(Mcp,…)；`request_complete_task` → dispatch(Mcp, CompleteTask) → 返回 NeedsUserConfirmation 语义（`ok=true`，payload 说明「待孩子确认」），**绝不直接 emit Complete**；
   - 状态名辅助（TaskStatus/SessionStatus → 字符串）、JSON-lite 最小转义（.cpp 匿名命名空间）。

### 必测场景（learning_mcp_host_tests.cpp）

1. get_today_tasks 返回快照中的今日任务（数量/标题/status 字符串）。
2. get_current_task 有会话返回任务+会话状态；无会话 active=false。
3. get_remaining_time 复用 focus 数学：Running 段随 now 单调减少；Paused 不变；无会话 active=false。
4. get_today_progress 按 §4.6 规则（含全 Completed、含 Pending 排除、空表=0）。
5. start_task 带 task_id → dispatcher/sink 收到 Intent::StartTask（Mcp 源）。
6. pause_task/resume_task → Pause/Resume。
7. request_complete_task → sink **零 Complete 调用**，响应表达待确认；随后 dispatcher.confirmPendingComplete() 才 emit 一次 Complete。
8. 未知工具名 / 缺 task_id / 任务不存在 → ok=false + error 文本，sink 零调用。

### 验收与提交

- host gate compile/link/run PASS（含既有全部非回归），新测试 exit 0。
- 提交：`feat(WB-LEARNING-V4): learning mcp host 8 tools (P15)`；push 后记录 hash。

## 7. P16 — Platform Ports（8 抽象 + fakes）

### 目标

在 `firmware/main/ports/` 定义 8 个平台抽象 Port（纯接口），Learning Domain / interaction / mcp **一律不 include Metalio/ESP-IDF**；提供确定性 fake 供 host 验证。接口最小化，P17 设备适配时再按真实边界扩。

### 允许修改

- 新增 `firmware/main/ports/**`：`clock_port.h`、`storage_port.h`、`network_port.h`、`ui_port.h`、`voice_session_port.h`、`stt_port.h`、`mcp_registration_port.h`、`power_port.h`
- 新增 `firmware/tests/fakes/fake_platform_ports.h`、`firmware/tests/unit/ports/ports_contract_tests.cpp`
- `tools/dev/verify-host-cpp-tests.ps1`（implRoots 增 `ports`；另加 4b3 include 扫描门禁，详见 §8）

### 必须实现（最小签名，注释标注 P17 扩展点）

1. ClockPort：`epochSeconds()`、`monotonicMs()`、`isTimeSynced()`。
2. StoragePort：KV 持久化 `read(key)->optional<string>`、`write(key,value)->bool`、`erase(key)->bool`、`contains(key)`。
3. NetworkPort：`struct NetworkStatus{bool online; std::string ssid;}`、`status()`；`postJson(url, body, timeout_ms, out_response)` 返回 `bool`（P17 才做真实 HTTP；MVP 只留边界与 fake）。
4. UiPort：`showScreen(ui::Screen)`、`showConfirm(title, onConfirm(std::function<void(bool)>))`、`toast(text)`（LVGL 归 P17）。
5. VoiceSessionPort：`beginSession()`/`endSession()`（对应 Metalio `SetVoiceUiDesired` 语义边界，P17 桥接）。
6. SttPort：`setEnabled(bool)`、`setResultCallback(std::function<void(std::string)>)`（MVP 只留边界）。
7. McpRegistrationPort：`registerTool(name, std::function<std::string(const std::string&)>) -> bool`、`registeredNames()`（P17 桥到 Metalio AddTool）。
8. PowerPort：`struct PowerInfo{int battery_percent; bool charging;}`、`info()`、`stayAwake(bool)`。

fake：全部确定性、可注入（battery/online/ssid 可设），记录调用计数供断言。ports 接口头允许 include `<functional>/<optional>/<string>/<cstdint>` 与 `ui/view_state.h`（UiPort 引用 Screen 枚举）等 host-safe 头。

### 必测场景（ports_contract_tests.cpp）

1. 8 个 fake 均可实例化并完成一次基本往返（write→read、set→get、begin→end、register→registeredNames 等）。
2. StoragePort fake：写覆盖、erase 后 read 为空、失败注入。
3. NetworkPort fake：online 翻转影响 status、postJson 结果可注入。
4. PowerPort fake：battery/charging 注入反映到 info。
5. UiPort fake：showConfirm 回调可按注入的 bool 触发。

### 验收与提交

- host gate PASS（含 4b3 include 扫描：interaction/mcp/ports 不得 include lvgl/esp_/freertos/driver/bsp/wifi/nvs/hal/metalio）。
- 提交：`feat(WB-LEARNING-V4): platform ports interfaces + fakes (P16)`；push 后记录 hash。

## 8. Host 门槛与报告收口（每个 checkpoint 均执行）

- `tools/dev/verify-host-cpp-tests.ps1`：C++17 `-Wall -Wextra -Werror` compile/link/run；宿主编译器 `E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe`；接口交叉契约 `...riscv32-esp-elf-g++.exe` 继续 PASS（非回归）。
- P16 起新增 §4b3 扫描门禁到 `verify-host-cpp-tests.ps1`，把 interaction/mcp/ports 纳入禁止 include 检查；随后各 checkpoint 全量重跑。
- 收口：`docs/project_management/reports/WB-LEARNING-V4_REPORT.md` 增补阶段 2 章节（8 要素、验证命令/结果、风险、范围偏差=无）；TASK_BOARD 增 6.18 行并随 checkpoint 更新；`git diff --check` PASS；普通 push 后返回 checkpoint-ready，等待用户 ACCEPT 决策。

## 9. 范围边界与后续（本阶段外）

- §10 Metalio Adapter（`integration/metalio_claw4/` thin adapter：presenter→LVGL、Touch→Interaction、Clock/Storage/Network/STT/SetVoiceUiDesired/MCP registration 桥）→ 设备侧，待用户授权。
- 真实 MCP transport（Metalio `ParseMessage`/`AddTool`、JSON 编解码）、VoiceSession 真实 I2S/唤醒词互斥、真实 HTTP/TLS、真机 App Shell → `HARDWARE_VERIFY_REQUIRED`/另行授权。
