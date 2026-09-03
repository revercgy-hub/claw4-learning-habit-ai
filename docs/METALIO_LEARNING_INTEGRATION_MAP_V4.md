# Metalio Learning Integration Map V4（V4 §6 / 任务规划 V4 P13）

- 依据：`项目总规划/WORKBUDDY_CLAW4_学习伙伴_完整开发提示词_V4.md` §6、`项目总规划/CLAW4_学习伙伴_任务规划_V4.md` §5（P13）
- 日期：2026-09-03（WorkBuddy，WB-LEARNING-V4-HOST）
- 源码基线：`vendor/MetalioClaw4` @ `ca3aa3fa`（tag `latest`；只读）；本文代码引用基于其同源构建镜像 `E:\c`（中文路径规避镜像），两者同源
- 证据约定：`SOURCE_CONFIRMED`=官方源码声明；`ARCH_DECISION`=本文设计意图（未实现）；`HARDWARE_VERIFY_REQUIRED`=需实机确认；`UNKNOWN`=无可靠证据

## 1. 集成模型总览（结论先行）

Metalio 官方固件已把 XiaoZhi 的"应用"演进为 **LVGL Screen 应用架构**：

- 每个功能（聊天/音乐/计算器/天气/相机/2048…）是 `display/screen/<name>_screen/` 下的一个 Screen 类；
- **Home 主屏**（`home_screen`）以**手机式 3×3 图标网格**（`AppEntry` 表：`LaunchFn launch` + lifecycle 回调）注册并启动各应用，点击图标 → 执行 `LaunchFn` → 返回全屏 `lv_obj_t*`；
- 每个应用通过 `screen_attach_lifecycle(scr, cb)` 挂载 **LOAD / UNLOAD 生命周期钩子**（screen 成为活动屏时 LOAD、LVGL 切走时 UNLOAD）；
- Metalio 官方已内置 **`openclaw_screen`**（Home 网格引用 `OpenClawScreen::LifecycleCallback`），证明"官方原生 App 加入 Home 网格"是该平台的既有扩展路径。

> **Learning App 建议形态（`ARCH_DECISION`）**：作为 Home 网格内新增的原生 Screen（`learning_screen`），复用 `screen_attach_lifecycle` 生命周期，UI 逻辑对接 WB-STREAM-002 已验收的纯 presenter 层（Home/Focus/Done/Offline），把 Learning Core（已 HOST_VERIFIED）作为平台无关库链入 `main`。**在真机与官方基线事实（含 openclaw_screen 边界）确认前不实现（HOLD）。**

## 2. 逐项确认表（V4 §6 / P13 清单）

| # | 确认点 | 结论 | 官方源码事实（SOURCE_CONFIRMED） | 集成含义 / 动作 |
| --- | --- | --- | --- | --- |
| 1 | Home app registration | ✅ 机制存在 | `main/display/screen/home_screen/home_screen.cc`：Home 3×3 图标网格；`AppEntry` 表（`LaunchFn launch`，`nullptr=no action`）；逐个 `XxxScreen::Create()` 返回全屏 `lv_obj_t*` | Learning Screen 需在 AppEntry 表加一项（图标 + LaunchFn + lifecycle cb） |
| 2 | Screen lifecycle | ✅ 机制存在 | `main/display/screen/screen_util.{h,cc}`：`screen_attach_lifecycle(lv_obj_t* scr, screen_lifecycle_cb_t cb)` → LOAD（新屏激活后）/ UNLOAD（LVGL 切走时）通知；`home_screen.cc` 顶部注释明确"这是挂接应用 start/stop 行为（如离开播放页暂停音频）的正确位置" | Learning 会话计时/暂停/落盘事件应在 UNLOAD/LOAD 处衔接（对应 Focus 页暂停语义） |
| 3 | LVGL ownership | ✅ 单线程模型 | `main/display/lvgl_display/lvgl_display.h`（`LvglDisplay : Display`）、`lv_adapter_display.*`；屏幕对象均为 `lv_obj_t*`（LVGL screen，parent=NULL） | Learning UI 适配层必须以 LVGL 线程调用；presenter→LVGL 映射保持单向（复用 CP4 纯 presenter 28/28 资产） |
| 4 | Application::Schedule | ✅ 机制存在 | `main/application.h:48` `void Schedule(std::function<void()> callback);`（主循环事件 `MAIN_EVENT_SCHEDULE` 等事件组驱动，`application.h` 事件位定义） | 跨线程回调（协议/后端事件 → UI）经 `Application::Schedule` 投递到主循环；Learning 事件发布同此路径 |
| 5 | STT | ✅ 走 XiaoZhi 云端 ASR 模型 | `main/audio/audio_service.{h,cc}`、`processors/afe_audio_processor.*`（AFE/VAD 前端）、`audio/wake_words/`；`SetVoiceUiDesired` 语义（见 #9） | Learning MVP 以文本输入（家长 PWA 建任务）为主，不依赖设备 STT；如需语音指令再评估（Device MVP，HOLD） |
| 6 | MCP ParseMessage | ✅ 机制存在 | `main/mcp_server.h:326` `void ParseMessage(const cJSON* json);` / `void ParseMessage(const std::string& message);` | Learning MCP Host（V4 §8/§20 L5）的设备端消息入口；本轮仅记录，不实现 |
| 7 | MCP tool registration | ✅ 机制存在 | `main/mcp_server.h:323` `AddTool(McpTool* tool)`、`:324` `AddTool(name, description, properties, callback)`、`:341` `std::vector<McpTool*> tools_` | Learning 工具（如"查今日任务"）以 `AddTool` 注册；本轮仅记录，不实现 |
| 8 | SetVoiceUiDesired | ✅ 机制存在且语义明确 | `main/application.h:66-71`：语音 UI 会话（聊天页/数字人页）级开关；`desired=false` → 软停 Feed / disable_wakenet，延迟硬 destroy AFE（复用引擎防低内存重建崩溃）；`IsVoiceUiActive/IsVoiceUiDesired` | Learning 屏若占用音频通道需与聊天/数字人页互斥；退出 Learning 语音会话走同一软停路径（ARCH_DECISION） |
| 9 | Storage | ✅ NVS 为主 | `main/settings.{h,cc}`（`nvs_flash.h`，`nvs_handle_t`）；存在 `SdCardScreen`（SD 卡应用）；官方 OTA URL 默认存 NVS `wifi/ota_url` | 设备端学习状态/未同步事件落盘需评估 NVS 键值 vs SD（WB-STREAM-002 outbox 的平台适配点；真实 NVS 适配属 Device MVP，HOLD） |
| 10 | Network | ✅ esp_wifi + 协议双传输 | `main/protocols/{websocket,mqtt}_protocol.*`；Wi-Fi 配网 UI（`network_screen`）；`system_info.cc` 引用 esp_wifi；NVS 存 wifi 配置 | 学习事件同步（WB-STREAM-002 coordinator/outbox 已 HOST_VERIFIED）待接设备网络层；设备 TLS/CA 路径 `HARDWARE_VERIFY_REQUIRED` |
| 11 | build / app-flash | ✅ 既有流程 | `E:\c` 构建镜像；`idf.py build` + `app-flash`（仅 ota_0 应用分区）；出厂固件 `Metalio_Claw4_Latest.bin`（33,120,256 B，SHA-256 前缀 `1a69e379`） | Learning App Shell 进入真机前的构建门禁沿用 V4 §12；**每次写 Flash 需用户授权（BLK-FLASH-AUTH-001）** |
| 12 | ota_0 size margin | ⚠️ 余量紧张 | 分区假设 ota_0≈9 MiB（9,437,184 B）；当前应用 `xiaozhi.bin` 9,036,192 B（≈8.62 MiB）→ **余量 ≈ 400,992 B（≈0.38 MiB）**；`xiaozhi.bin` 放不进 ota_1（4 MiB）；**ota_1 已有官方用途** = ESPClaw 本地 edge_agent（README §10.1，另需 emote/system/storage 分区） | Learning 代码/资源体积预算极紧（~0.38 MiB 量级）；**严禁改分区表、严禁占用 ota_1**（官方 edge_agent 槽）；超预算需重新做体积/裁剪决策并经用户授权 |

## 3. 与现有资产的关系（不重复造轮子）

- **Learning Core（domain reducer / outbox / coordinator）**：`firmware/main/learning_domain/`、`firmware/main/application/`（现为纯主机、零硬件头，HOST_VERIFIED）→ 计划作为平台无关 C++ 库链入 Metalio `main`；其 include 洁净性已被 CP0~CP8 门禁保护（forbidden include 扫描 PASS）。
- **UI presenter（Home/Focus/Done/Offline）**：`firmware/main/ui/presenters.{h,cpp}`（纯映射、28/28）→ Learning Screen 的视图模型源；Screen 层只做 LVGL 渲染适配，不改 presenter。
- **MCP / Voice / Audio / Protocol**：本轮不实现；集成点已在 §2 表锁定，避免后续重复侦查。
- **openclaw_screen（官方内置，2026-09-03 源码级核实）**：`SOURCE_CONFIRMED` = Metalio **云 AI Agent 平台**（OpenClaw）的设备端 App（README §2.2/§10；`openclaw_screen.h`）。走 HTTP API（`main/api_endpoints.h`：`/api/v1/devices/status`、`/conversation`、`/conversation/{id}/messages`、`removeAll`），按住说话录音上传（独立 FreeRTOS task 不阻塞 LVGL）+ 消息气泡 UI，Home 网格经 `LifecycleCallback` 接入。**结论：OpenClaw 是通用云 Agent 对话入口，不是学习习惯宿主；Learning 与 chat / openclaw / digital_people 等为 Home 网格并列 App。** 官方 App 全清单见 README §11（`home_screen.cc` → `kApps[]`）。
  - **可复用参考模式（Learning screen 实现时照做）**：① mic 与唤醒词共用 I2S 通道 → 录音前后必须临时关 wake word，且 `LifecycleCallback` 兜底（退出屏幕时还原）；② 录音/上传/网络放独立 FreeRTOS task，不阻塞 LVGL；③ 设备状态非 `Idle`（正在 AI 对话）时按钮置灰拒绝触发；④ 进入前激活检查（未激活弹不可关闭拦截窗）。
  - **ESPClaw（README §10.1，与 Learning 无关但约束分区认知）**：Home 的 ESPClaw 入口 boot `ota_1` 内的本地 edge_agent（另需 emote/system/storage 分区，见 `esp_claw_bin/README.md`）→ 印证 ota_1 已有官方用途（本地 agent），**Learning 不得占用 ota_1 或改分区**；MVP 不开发本地大模型/edge agent。

## 4. 风险与门禁

- `HARDWARE_VERIFY_REQUIRED`：屏幕/触摸/LVGL 实机渲染、flash 实际分区与容量、Wi-Fi/TLS、音频 codec、openclaw_screen 行为——全部待真机；本文任何源码事实**不等于实机已确认**。
- 真机写 Flash（app-flash / 分区/OTA）仍受 `BLK-FLASH-AUTH-001` 门禁，须用户逐次授权。
- ota_0 余量 ≈0.38 MiB 是硬约束：Learning App Shell 阶段若超预算，须先裁剪或经用户授权调整方案，**不改分区表**（永久禁区）。
- 本流（WB-LEARNING-V4-HOST）只产出本文（集成地图），**不实现任何集成代码**；Learning Screen / adapter / app-flash / L0~L6 在用户授权 + 真机就绪后按 V4 §11~§21 推进。
