# XiaoZhi Upstream Tracking（V4 §5 / 任务规划 V4 P12）

- 依据：`项目总规划/WORKBUDDY_CLAW4_学习伙伴_完整开发提示词_V4.md` §5、`项目总规划/CLAW4_学习伙伴_任务规划_V4.md` §4（P12）
- 日期：2026-09-03（WorkBuddy，WB-LEARNING-V4-HOST）
- 基线：planning-v4 `1310ca3d`；Metalio 官方只读基线 `vendor/MetalioClaw4` @ `ca3aa3fa`（tag `latest`；构建镜像 `E:\c`，两者同源）
- 上游：[XiaoZhi AI (xiaozhi-esp32)](https://github.com/78/xiaozhi-esp32)

## 1. 目的与铁律

目的：把 XiaoZhi 上游演进**作为受控输入**追踪，而非照单全收。

铁律：

1. **禁止整仓 merge XiaoZhi**。MetalioClaw4 已是深度定制 fork（P4/C5 双芯片、蓝牙音频 codec、双屏/副屏、独立 OTA 通道等），整仓 merge 会引入与 `metalio-claw-4` 板无关的回归风险。
2. 只做 **selective backport**：单主题、单提交、带回归证据、落 `workbuddy/` 工作分支，经评估后由用户/看板决策是否并入。
3. 上游安全 / 输入校验类修复**优先级最高**，一旦确认适用立即评估回移。
4. Metalio 特有实现（见 §4 `KEEP_METALIO` 行）**不回推也不替换**，除非上游出现同类能力且证明更优并经用户决策。

## 2. 上游事实基线

| 项 | 事实 | 证据 |
| --- | --- | --- |
| Metalio 与 XiaoZhi 关系 | Metalio Claw4 固件 "based on the XiaoZhi AI (xiaozhi-esp32) framework, customized for the `metalio-claw-4` board" | `vendor/MetalioClaw4/README.md:402` |
| Upstream architecture 声明 | 官方 README 明确列出 "Upstream Architecture: XiaoZhi AI (xiaozhi-esp32)" | `vendor/MetalioClaw4/README.md:12` |
| Metalio 官方基线提交 | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`（tag `latest`） | WB-001 平台映射；`docs/CLAW4_PLATFORM_MAP.md` |
| 仓库内 Metalio 源码 | `vendor/MetalioClaw4/`（只读基线，禁止向官方 origin 推送本项目变更） | 根 `AGENTS.md` §6 |
| Metalio 典型上游同构区 | `main/application.*`（事件组主循环）、`main/device_state.h`（DeviceState 枚举）、`main/protocols/{websocket,mqtt}_protocol.*`、`main/mcp_server.*`、`main/display/*` | `vendor/MetalioClaw4/main/` 目录结构 |
| Metalio 深度定制区 | `main/audio/*`（蓝牙 codec 三模式，替代上游 ES8311+ES7210）、`secondary_screen/`、`usb_extend_screen/`、`components/usb_device_uac`、`boards/metalio-claw-4`、`esp_claw_bin/`、`xingzhi-assets/`、`factory-test-assets/` | `vendor/MetalioClaw4/README.md` §12.1；目录结构 |

追踪方法（每次评估时执行）：以 GitHub `78/xiaozhi-esp32` 的 release note / commits 为输入 → 对照本文件 §4 分类 → 命中 `BACKPORT_CANDIDATE` 才进入评估。

## 3. 分类定义

| 分类 | 含义 | 动作 |
| --- | --- | --- |
| `KEEP_METALIO` | Metalio 特有/深定制实现，上游不适用 | 不回推、不替换；仅当上游出现同类能力且经用户决策才重评 |
| `TRACK_UPSTREAM` | 与上游同构、需持续跟踪演进 | 记录差异；不主动改 Metalio；上游修复/改进出现时重新评估 |
| `BACKPORT_CANDIDATE` | 上游修复/能力**确认适用**且价值明确 | 进入 selective backport 流程（§5），单提交回移并回归 |
| `HOLD_MIGRATION` | 上游大改/重构，迁移成本高、收益未定 | 暂缓；记录触发条件与评估日期，不投入改动 |

## 4. 逐域分类判定（V4 重点域）

> 代码列指 Metalio 仓库内路径（`vendor/MetalioClaw4/main/...`）；"状态"为 2026-09-03 判定，随上游演进定期重评。

| 域 | Metalio 侧落点 | 分类 | 判定理由 | 建议动作 |
| --- | --- | --- | --- | --- | --- |
| MCP | `mcp_server.{h,cc}`、`application.cc` 集成 | `TRACK_UPSTREAM`（Learning MCP Host 落地时升 `BACKPORT_CANDIDATE`） | 设备端 MCP server 机制与上游同构；Learning MCP Host（V4 §8/§20 L5）将依赖该协议面 | 保持跟踪；不提前改动 |
| AudioService | `audio/audio_service.{h,cc}`、`audio_codec.cc`、`codecs/`、`processors/` | `KEEP_METALIO` | Metalio 用蓝牙音频 codec 三模式替代上游 ES8311+ES7210 分立方案（README §12.1），上游音频管线不直接适用 | 不整块跟随；仅上游公共 bugfix 按单点评估 |
| Protocol（协议层） | `protocols/protocol.{h,cc}`（基类/编排） | `TRACK_UPSTREAM` | 协议编排与上游同构；上行/下行事件语义演进需对齐 | 记录 diff；上游协议字段增改时重评 |
| WebSocket / MQTT | `protocols/websocket_protocol.*`、`mqtt_protocol.*` | `TRACK_UPSTREAM` + 安全修复自动升 `BACKPORT_CANDIDATE` | 传输实现与上游同构；断线重连/心跳/加密握手的安全修复应尽快回移 | 上游 security/连接管理修复优先评估 |
| ESP-SR（唤醒/ASR 前端） | `audio/wake_words/`、`audio/wake_word.h`、组件依赖 | `TRACK_UPSTREAM` | 依赖 ESP-SR 组件版本（managed_components/依赖清单）；唤醒词模型与音频链路 Metalio 定制 | 组件版本随上游 bump 时回归唤醒与 VAD 链路 |
| P4 / C5 fixes | `boards/metalio-claw-4/`、ESP32-P4 + C5 双芯片代码 | `KEEP_METALIO`（上游 P4 相关修复除外） | xiaozhi-esp32 主流目标为 ESP32-S3；Metalio 的 P4 主控 + C5 副控是核心差异化 | 上游若出现 P4 专属修复 → 单项升 `BACKPORT_CANDIDATE` |
| security / input validation | 协议解析、`settings.*`、OTA/升级路径 | `TRACK_UPSTREAM`（命中即回移评估） | 上游输入校验/安全修复与板级无关，普适性最高 | 最高优先级跟踪；适用即 selective backport |
| DeviceStateMachine | `device_state.h`（Starting/WifiConfiguring/Idle/Connecting/Listening/Speaking/Upgrading/Activating/AudioTesting/FatalError） | `TRACK_UPSTREAM`（状态语义）+ `KEEP_METALIO`（Learning 扩展点） | 枚举与上游基本同构；Learning 需在协调器层扩展状态，不直接改 XiaoZhi 状态机 | 状态语义演进跟随；Learning 状态作为领域层扩展 |

### 4.1 补充：Metalio 特有区清单（`KEEP_METALIO`，不整仓跟随上游）

- 蓝牙音频三模式（对话 / 蓝牙耳机音箱 / 手机蓝牙音箱，README §12.1）
- `secondary_screen/`、`usb_extend_screen/`、`main/usb_extend_screen`、`components/usb_device_uac`（USB UAC）
- `boards/metalio-claw-4` 板级定义与 `Kconfig`
- OTA 通道：默认 `https://api.tenclass.net/xiaozhi/ota/`（`settings ota_url` 可覆盖；`docs/CLAW4_PLATFORM_MAP.md`）
- 显示：`display/` 多适配（lcd / lv_adapter / lvgl / emote / oled / screen / touch_feed）
- 资产与出厂：`xingzhi-assets/`、`factory-test-assets/`、`Metalio_Claw4_Latest.bin`（33,120,256 B，SHA-256 前缀 `1a69e379`）

## 5. Selective Backport 流程纪律（触发时执行）

1. **识别**：上游 commit/release 命中 §4 `BACKPORT_CANDIDATE` 或安全类修复。
2. **适用性评估**：确认与 `metalio-claw-4`（P4+C5、蓝牙音频、双屏、自定义 OTA）无冲突；检查是否触碰 `KEEP_METALIO` 区。
3. **落地**：在 `workbuddy/` 工作分支单主题提交（提交信息 `fix(<task-id>): backport <upstream sha> <主题>`），不整块 merge。
4. **验证**：按根 `AGENTS.md` §5 保留证据（本机编译/单测/回归）；涉及协议/安全改动需覆盖回归。
5. **决策**：WorkBuddy 报告 → 用户/看板决策是否并入发布基线；禁止绕过看板直接并入 `main`。
6. **记录**：每次评估与回移结果回写本文件 §4 状态列。

## 6. 追踪执行与重评节奏

- 建议节奏：**每周**或上游有 release/安全公告时，对照 GitHub release notes 重跑 §4 分类；安全修复即时评估。
- 触发重评的信号：上游新增设备端 MCP/Agent 协议、协议字段增改、连接管理/安全修复、ESP-SR 组件大版本、P4 target 支持变化、OTA/升级路径变化。
- 当前（2026-09-03）无进行中 backport；全部域处于跟踪/保持状态。

## 7. 待核实与风险

- 上游当前 commit 与 Metalio `ca3aa3fa` 基线的**精确差距清单**未在本地落盘（需联网对 `78/xiaozhi-esp32` 执行 compare 后才能给出，本轮未执行网络 compare）。
- Metalio 上游仓库后续 release 的分叉点未知；建议首次联网评估时记录上游 commit 与 `ca3aa3fa` 的关系。
- 本文件只做策略跟踪，不替代任何实机验证；涉及真机/Flash/OTA 的改动仍需用户授权（`BLK-FLASH-AUTH-001`）。
