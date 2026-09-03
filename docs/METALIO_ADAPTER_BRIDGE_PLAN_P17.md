# Metalio Adapter 设备边界桥接方案（V4 §10 / P17）—— 设计草案（供授权决策）

> 状态：`DRAFT`（纯文档，**未创建任何代码/文件于 `integration/metalio_claw4/`**）
> 依据：V4 §9/§10、`docs/METALIO_LEARNING_INTEGRATION_MAP_V4.md`（源码级证据）、`docs/XIAOZHI_UPSTREAM_TRACKING.md`、`firmware/main/ports/*`（P16 已验收抽象）、`firmware/main/interaction|mcp`（P14/P15 已验收）
> 本文用于：在你授权 P17/§10 前，呈现 adapter 将触碰的 Metalio 接入点、桥接语义、授权门禁与验收方式。凡标注 `HARDWARE_VERIFY_REQUIRED` 的项一律以真机实测为准。

## 1. 设计原则（承继 §6 结论）

1. Learning 是 Home 3×3 网格中的**原生并列 App**（非宿主）：adapter 只做**薄桥**，不复制 Metalio 的 BSP / Audio / Network / MCP service / Screen 实现。
2. `vendor/MetalioClaw4/**` 保持只读基线：所有桥接以「adapter 调用官方公开入口」实现，官方源码零改动。
3. 不改 partition / ota_0 / ota_1：Learning 仅用 app 分区既有余量（约 0.38 MiB），禁占 ota_1（ESPClaw 在用）。
4. Learning Domain / interaction / mcp / ports 保持零 Metalio/IDF 头（4b3 门禁持续生效）；Metalio 类型只允许出现在 `integration/metalio_claw4/` 这一层。

## 2. 授权门禁（未解除前禁止创建 adapter 文件）

| 门禁 | 状态 | 说明 |
| --- | --- | --- |
| 真机连接 | 未连接 | 串口/USB 枚举、屏幕/触摸/Flash/PSRAM/C5/音频全部 `HARDWARE_VERIFY_REQUIRED` |
| 刷写/擦除 Flash | `BLK-FLASH-AUTH-001` 未解除 | 需用户明确授权后才刷自定义固件 |
| 修改 partition/Bootloader/OTA | 禁止 | ota_0 余量≈0.38 MiB；ota_1 归 ESPClaw |
| vendor 源码修改 | 禁止 | 只读基线 |
| 引入真实 STT/ASR 引擎 | HOLD | MVP 语音走固定短语表（P14 STT mapper 已主机验证）；真 STT 引擎待产品决策 |

## 3. Port → Metalio 桥接映射（每项含接入点与风险）

| Learning 接口（P16 已验收） | Metalio 接入点（证据） | 桥接语义 / 风险 |
| --- | --- | --- |
| `ClockPort::epochSeconds/isTimeSynced` | SNTP/RTC（官方网络层）；本地日边界逻辑已由 backend `local_day_epoch_bounds` 验证 | adapter 向 `ReducerContext` 注入真实时钟；未同步时间用 `TimestampSource::Local`。真机 RTC/SNTP 时序 `HARDWARE_VERIFY_REQUIRED` |
| `ClockPort::monotonicMs` | `esp_timer_get_time()`（官方应用计时） | 会话段结算单调源；注意与官方 UI 计时互不干扰 |
| `StoragePort`（KV） | NVS `settings` 命名空间（官方持久化设置证据） | outbox/domain 快照的设备侧后端；键名空间前缀隔离（如 `claw4_`），防与官方设置冲突。真实 NVS 读写 `HARDWARE_VERIFY_REQUIRED` |
| `NetworkPort::status/postJson` | Wi-Fi 事件 + `esp_http_client`（TLS） | 桥到 WB 家庭后端契约（backend 已存在 70/70）；MVP 同步=单 JSON POST。真实 Wi-Fi/TLS `HARDWARE_VERIFY_REQUIRED` |
| `UiPort::showScreen` | Home 网格 `kApps[]` + `screen_attach_lifecycle` LOAD/UNLOAD（§6 证据） | Learning screen 用官方 screen 机制注册；LVGL **单线程**——一切上屏走 `Application::Schedule`。Home 3×3 需加 Learning 入口（官方 registry 文件属 vendor 只读→**入口注册方式需在授权后先做最小验证**） |
| `UiPort::showConfirm/toast` | LVGL 弹窗/提示（learning screen 内自绘，不复制官方控件） | 完成确认 UX 落地（见 §4）；真机渲染 `HARDWARE_VERIFY_REQUIRED` |
| `VoiceSessionPort::begin/end` | `SetVoiceUiDesired(bool)` 会话级软停（§6 证据） | 学习专注/录音前后接管 mic 权；唤醒词互斥与 LifecycleCallback 兜底还原照 openclaw 参考模式（§6 已核实） |
| `SttPort` | ESP-SR / 官方 STT 边界（未定） | MVP 首版可不接真 STT：语音命令由固定短语表模拟（P14 host 已验证）；真机接入列为后续项 |
| `McpRegistrationPort::registerTool` | `McpServer::AddTool(name, handler)` + `ParseMessage`（§6 证据） | 注册 8 个 `learning.*` 工具；`ParseMessage` 命中后调 `LearningMcpHost::invoke`（args JSON-lite→结构体→invoke→JSON-lite 响应）。**JSON 编解码器在 adapter 层实现**（P15 只产出 JSON-lite 字符串） |
| `PowerPort::info/stayAwake` | 电源管理 IC/`esp_pm`（真机 SKU 未确认） | 专注期间 `stayAwake(true)`；电量低提示。电池/充电状态 `HARDWARE_VERIFY_REQUIRED` |

## 4. 完成确认 UX（AI 请求 → 物理确认 → 域提交）设备落地序列

```text
AI/云端 → ParseMessage("learning.request_complete_task", {task_id})
  → McpRegistrationPort handler → LearningMcpHost.invoke
  → CommandDispatcher.dispatch(Mcp, CompleteTask)   [只登记 pending，零域变更]
  → McpResponse {"confirmation":"pending"}           [回 AI：已请求孩子确认]
  → UiPort.showConfirm("完成任务？", on_confirm)      [Learning screen 弹窗]
  → 孩子点「确认」(Touch 源)
  → CommandDispatcher.confirmPendingComplete()        [唯一发 Intent::Complete 的出口]
  → CommandSink → AppCoordinator.dispatchIntent       [outbox 原子提交]
  → 域状态 Task=Completed；active_session 清空（Done 语义）
```

- 弹窗期间重复 AI 请求不重复 emit（P14 测试锁定）。
- 「取消」→ `cancelPendingComplete()`，域零变更。
- 触摸「完成」按钮（Touch CompleteTask）本身即物理确认，等价消费 pending。

## 5. 同步与恢复闭环（adapter 承担的胶水职责）

1. 启动：`StoragePort` 载入 outbox/domain 快照 → `AppCoordinator` 恢复 → 若有 intact 快照走 `recoverSession` 决策屏（UiPort）→ `endRecoveredSession`（用户显式保存）或继续。
2. 在线：`NetworkPort.postJson` 承载 `SyncTransport`；`runSyncOnce` 按已验收策略（连续前缀、单次 re-auth、60s 退避上限）推进。
3. 离线：今日任务缓存照常可 Start（backend 今日任务 Ready 契约）；Dashboard 本地日归属逻辑已在 backend 验证。
4. `CommandSink`/`LearningBackend` 的 production 胶水（现仅存在于 host 测试 `CoordSink/CoordBackend`）由 adapter 在同一层实现，保持薄。

## 6. 建议实施次序（授权后按此推进，每步独立 checkpoint）

| 步骤 | 内容 | 前置 |
| --- | --- | --- |
| P17a | `integration/metalio_claw4/` 骨架 + Clock/Storage/Network 三桥（纯 host 可先联调的部分用 fake 真值替身） | 用户授权 + 真机连接评估 |
| P17b | UiPort → Learning screen（含 confirm 弹窗）桥 | P17a；真机可渲染 |
| P17c | McpRegistrationPort 桥（AddTool/ParseMessage + JSON 编解码） | P17a/b |
| P17d | VoiceSession/Stt（mic 互斥 + 固定短语模拟口） | 真机音频 `HARDWARE_VERIFY_REQUIRED` |
| P18 | 首版 Learning App Shell（Home→Learning→Mock Task→Start→Back，原厂 Chat/Settings/Test 不受影响） | P17 验收 + `BLK-FLASH-AUTH-001` 解除 |

## 7. 验收与禁忌（adapter 阶段）

- 验收：每桥独立可测（host 侧用 fakes 的替身注入真值；真机侧逐项 `HARDWARE_VERIFY_REQUIRED` 记录）；全量 host gate + E2E 不回归；vendor `git diff` 为空。
- 禁忌：不改 vendor；不复制官方 service/screen 实现；不占 ota_1/不改分区；不实现摄像头/情绪识别/本地大模型/4G·GPS（MVP 外）；所有 MCP 变更仍走 P14 完成门禁（AI 不直接 Complete）。

## 8. 待用户决策（本文档目的）

1. 是否 ACCEPT 阶段 2（P14/P15/P16 + 证据链）；
2. 是否授权按 §6 次序进入 P17a（Metalio Adapter 骨架 + Clock/Storage/Network 桥）——本文档即其输入；
3. 真机连接与 `BLK-FLASH-AUTH-001` 解除的时机安排。
