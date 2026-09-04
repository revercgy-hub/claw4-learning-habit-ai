# WB-LEARNING-V4-L1 — Learning App 设备基本功能（Device Basics）

> **2026-09-04 收口更新：** `DEVICE_L1C_PERSISTENCE=PASS` / `CHECKPOINT_READY（验收决策归用户）`，ready 代码 `4db2283`。用户完成屏侧测试；物理交互期间 monitor 未持续连接，因此 L1c 原“逐步 monitor 日志级 PASS”证据格式未完全采集，改由只读 NVS 法证（连续 sequence、有效唯一 event ID、六类 transition、复位后可读）支持限定的本地持久化通过。完整限制见 `reports/CODEX_WB_LEARNING_V4_L1_DEVICE_TEST_2026-09-04.md`。本任务包不再是活动工作流。

- Task ID: WB-LEARNING-V4-L1
- 分支: `workbuddy/learning-v4-host-sync`（当前唯一活动工作流分支，普通快进）
- 基线: L0 收口头（C28 后 current_remote_head 见 TASK_BOARD）
- 授权: 用户 2026-09-03 指示「先把学习 app 的基本功能构建起来，再上机测试」；真机沿用 L0 批次授权规则（**仅 ota_0 application app-flash + monitor**，无 erase/partition/ota_1/C5）
- 权威文档: `AGENTS.md`、`docs/ARCHITECTURE.md`、`docs/METALIO_ADAPTER_BRIDGE_PLAN_P17.md`、报告 §12–§15

## 1. 目标（一句话）

把已验收的主机侧业务核心（P14 漏斗 / P15 MCP host / P16 ports / FIX-V4 契约修正 / P17a glue）**编入真机固件**，用真实 NVS 持久化 + esp_timer 时钟替换 L0 的 UI Mock，使 Learning 屏展示**真实 DomainState 驱动的任务流**：进入 → 看到今日任务（权威快照缓存）→ Start（真实 reducer 事务：状态机迁移 + outbox 落 NVS）→ 运行中/暂停/恢复 → Complete（Touch 直发或确认门禁）→ 返回 → **重启后状态恢复**。

## 2. 范围

| 层 | 内容 | 放行 |
| --- | --- | --- |
| 设备核心编入 | `learning_domain/reducer.cpp`、`sync/outbox_core.cpp`、`application/coordinator.cpp`、`interaction/dispatcher.cpp`、`interaction/stt_mapper.cpp`(如被引)、`mcp/learning_mcp_host.cpp`、`ui/presenters.cpp` + 对应头 + `integration/.../host_glue/*` 镜像入 `E:/c/main/learning/` | ✅ |
| Device Ports（P17d 最小适配） | `ClockPort`←esp_timer（monotonicMs；epoch 用开机以来秒，`isTimeSynced=false` 占位）；`OutboxStorage`←NVS namespace `learning`（独立命名空间防与官方 Settings 冲突；840K nvs 分区充足） | ✅ |
| 序列化 codec | `OutboxState`（DomainState + pending + 计数器 + diagnostic）↔ blob 字符串（长度前缀 + 字段转义，纯 C++17，host 单测 roundtrip） | ✅ |
| 今日任务来源 | **seed 演示快照**（首次 boot 且 NVS 无 state 时经 `applyTodaySnapshot` 注入，版本字段标注 demo；L2 由真实 backend 快照替换） | ✅（临时，文档标注） |
| Learning 屏真实渲染 | 标题/今日任务列表（任务状态 chip）/主操作按钮（开始·暂停/继续·完成）/状态行（session 状态 + 实际秒数）→ 由 `LearningApp` state 派生，**不经 Mock** | ✅ |
| 完成确认门禁 | Complete 经 P14 门禁：Touch 直发；确认弹层语义保留（无 AI 时直接 Touch 路径） | ✅（P14 语义） |
| 重启恢复 | coordinator 启动时从 NVS load 旧 state，屏渲染恢复态；active Running 会话按 ARCHITECTURE 恢复语义处理 | ✅ |

**不包含（后续任务，需新授权/新 checkpoint）**：Wi-Fi/backend 真实同步（L2 起）、Voice/STT/MCP 通道上设备、NVS 官方命名空间改动、`sdkconfig` 改动、分区/ota_1/erase、C5、官方屏/HOME 之外的官方行为改动。

## 3. 关键架构决策

1. **权威副本 → 构建镜像**（沿用 learning_screen 先例）：repo `firmware/main/**`（业务核心）+ `integration/metalio_claw4/host_glue/*` 为**仓库权威**；`E:/c/main/learning/` 为**构建镜像**（同步脚本 `integration/metalio_claw4/device/sync_learning_tree.sh` 文档化），避免仓库中文路径进构建（同 vendor 镜像理由）。官方文件改动仅 `E:/c/main/CMakeLists.txt`（SOURCES 增 learning 源行 + include dir）→ **manifest 补丁 #3**。
2. **独立 NVS namespace `learning`**：仅读写自己的 key（`outbox_state` / `seed_done`），绝不触碰官方 namespace；nvs 分区 840K 够用；复用官方 `Settings` 类风格但自持 handle（少一次对象生命周期耦合）。
3. **序列化 codec 纯 C++17 且 host 可测**：任何持久化路径都先过 host roundtrip 测试，避免设备侧二进制调试。
4. **seed 只写一次**：`seed_done` 标记；之后即使 applyTodaySnapshot 被 L2 server 全量替换也不会复活 demo。
5. 状态刷新：屏不轮询；由 LVGL timer（~1 s）重读 `app_.state()` 派生渲染（渲染是无副作用的投影，符合 UI=presentation 层约束）。UI 唯一副作用入口 = `dispatcher.dispatch(CommandSource::Touch, payload)`。

## 4. Checkpoints

| # | 内容 | 完成定义 | 验证 |
| --- | --- | --- | --- |
| L1a | 任务包 + device core：`outbox_codec.{h,cpp}`（host 测试）、`learning_clock`、`nvs_outbox_storage`（NVS）、镜像同步 + manifest #3、CMake 挂载 | codec host gate 增套件全绿；`idf.py build` exit=0（learning 源编入）；sdkconfig diff=0 | host gate + idf build |
| L1b | Learning 屏真实渲染 + seed + 生命周期恢复 | 屏显示 NVS/coordinator 真实状态；Start→Pause/Resume→Complete（确认）驱动 state 变化且 log 断言；重启恢复路径 host/静态验证 | host gate + idf build |
| L1c | 真机上机（批次内）：seed→Start→Pause→Resume→Complete→Back→reboot 恢复 | monitor 日志级 PASS（事件/状态转换/持久化证据）+ 用户屏侧确认 | esptool ota_0 + monitor |

## 5. 验收标准（L1 整体）

1. `LearningApp` 在设备上以真实 NVS 为存储启动，无 seed 后重启任务/会话状态不丢（monitor 证据）。
2. 任务 Start 后 task.status=InProgress、session.status=Running 且 outbox pending=1（本地事件，重启保留）。
3. Complete 走 reducer 到 Completed + session Completed（Touch 路径）；事件在 pending 队列（等待 L2 同步上送）。
4. Learning 屏所有渲染均派生自 `app_.state()`（无 UI 本地业务状态残留——移除 L0 的 `s_ui.running/done` mock 语义）。
5. sdkconfig diff=0、partition 零改动、官方补丁仅 manifest #3（登记 APPLIED、可回滚）。
6. host gate 全绿（含新增 codec 套件）+ 跨语言回归不受影响。

## 6. 禁忌（重申）

- 不碰 sdkconfig / partition / bootloader / ota_1 / erase_flash / C5 / eFuse（任何此类操作需新授权）。
- 不改官方 NVS 命名空间/键；learning 数据只入 `learning` namespace。
- 不在屏代码里放业务逻辑（仅 dispatch/渲染投影）；不复制官方 service/screen 实现。
- 不引入第三方序列化库（codec 自持，纯 C++17）。
