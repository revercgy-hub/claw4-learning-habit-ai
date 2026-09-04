# WB-LEARNING-V4-L1 — 真机调试问题交接（HANDOFF）

> 状态：L1c 上机验证中，遇阻塞性缺陷（未解决）。本文件给接手的模型/工程师，含现象、硬证据、已排除项、待查项与建议路径。
> 时间：2026-09-04 16:45；分支：`workbuddy/learning-v4-host-sync`；真机 COM7（Espressif USB JTAG/serial）；批次授权：仅 ota_0 application app-flash + monitor（无 erase/分区/ota_1/C5）。

## 1. 现场现象（用户实机反馈）

- L1 固件（Learning 屏接真实状态机，bin 9,169,056 B 起）首刷 boot 正常、Home→Learning 正常、**seed 演示任务成功**（monitor：`LearningRt: first boot: demo today snapshot seeded=1`）。
- 用户点击 **「开始今日任务」→ 成功**（monitor：`touch kind=1 -> intent=0` Accepted，NVS `save blob=554 bytes set=0 commit=0`）；屏显示专注中。
- 随后点击 **「暂停」与「完成」均无反应**（用户反馈多次）——monitor：每次 `touch kind=2|4 -> intent=3`（**PersistFailed**）+ 屏 diag `persist-fail diag: tasks=2 active=1 pending=2`。
- 返回 Home / 重进屏正常；**重启后状态恢复**（boot log `boot with committed state: tasks=2 pending=2 active=1`，Running 会话与 pending 事件从 NVS 读回）。

## 2. 硬证据（monitor 关键日志行）

```
I LearningNvs: load peek=0 len=554        # NVS 读永远 OK（decode 成功）
I LearningScreen: touch kind=2 task=demo-math-001 -> status=0 intent=3   # Emitted + PersistFailed
I LearningNvs: load peek=0 len=554        # ×3（persistTransition loadState + …）
# —— 注意：失败动作后【从未出现 "save blob=…" 日志】——
I LearningNvs: save blob=554 bytes set=0 commit=0   # 只在 Start(成功) 时出现
```

结论：**每次失败动作，outbox 持久化从未执行到 `storage.commit()`（Save）** → `persistTransition` 在 commit 之前就返回失败；失败动作伴随 3 次 NVS load（成功）。

## 3. 已排除项（重要——别再走弯路）

| 候选根因 | 结论与证据 |
| --- | --- |
| NVS 读失败 / blob 损坏 | ❌ 排除：`load peek=0 len=554` 恒 OK；Start 的 save 成功 |
| NVS 写失败（空间/GC） | ❌ 排除：save 在 Start 时 `set=0 commit=0`（ESP_OK）；失败动作根本不触发 Save |
| 事件 id 重复（outbox duplicate 保护） | ❌ **已排除**：原实现用 NVS i64 计数器（`evseq`）生成事件 id，真机日志显示每次 `get=0`（计数器未持久化/异常）→ 事件 id 恒为 ev-1 → 重启恢复后与 pending[0] 撞 → 曾改 **esp_random() 63bit 随机 id**（learning_runtime.cpp `NextEventId/NextSessionId`，已 build+上机）→ **clean Start Accepted 后 Pause 仍 intent=3** → duplicate 非根因 |
| draft event_id 为空（reducer 侧） | ❌ 大概率排除：reducer `makeDraft()` 用 `ctx.make_event_id ? … : EventId{}` 兜底非空；Pause/Resume/Complete 全走 makeDraft；host 同源代码 Pause 测试通过 |
| pending 容量 >200 | ❌ 排除：pending=2 |
| reducer 拒绝（状态机） | ❌ 排除：拒绝返回 intent=1（RejectedInvalidState），实际是 3（PersistFailed） |

## 4. 矛盾点（下一个模型的主攻方向）

`persistTransition`（`firmware/main/sync/outbox_core.cpp`）在 commit 前只有 4 个失败出口（load 失败 / drafts 空 / capacity / duplicate event_id / draft id empty），**均被上述证据排除**，但设备上仍 PersistFailed 且无 Save 日志。可能方向：

1. **内存 `state_` 与 NVS 持久状态分歧**：coordinator 的 reducer 基于内存 `state_`，而 outbox `persistTransition` 开头重新 `load` NVS state 并做一致性计算。若二者 pending/next_sequence 不一致（如 device 上某处只更新了一方）→ 某个预检失败？——但代码路径里没有这种"比较不一致即失败"的逻辑（读代码确认过）。
2. **设备实际执行路径与 repo 源码差异**：已 diff `firmware/main/{sync,application}/*` 与镜像 `E:/c/main/learning/*` —— 完全一致；reducer/outbox/coordinator 均 SAME。
3. **建议下一步（直接可做）**：
   a. 在 `OutboxCore::persistTransition` 的每个失败出口（`makeFailure(...)`）加**设备可观测输出**——该文件是 host 共享代码不能含 esp_log，可用注入回调/或在 `NvsOutboxStorage`/屏 diag 打印更细（屏已打印 intent=3；需要的是区分 empty/duplicate/capacity/storage）。最直接：临时在镜像副本 `E:/c/main/learning/sync/outbox_core.cpp`（仅构建树）加 `ESP_LOGx` 重编译（**不改 repo 权威**，验证后同步结论）；或给 `CommandSinkGlue`/`LearningApp` 增加返回 pr.status 的通道。
   b. host 端构造"重启恢复 + 旧 pending"压力单测（fake storage 模拟设备 NVS 序列：seed→Start→模拟重启(同 disk)→Pause）验证 host 路径是否存在同类问题。
   c. 检查 `LearningApp`/`ContextBuilder` 的 `ctx_.make()` 每次是否为**同一份 factory**（怀疑 std::function 在值传递中被清空 → 但那会让 draft 空，已排除大半）。

## 5. 代码侧已落地的调试资产（保留，可移除）

- `integration/metalio_claw4/device/app/learning_runtime.{h,cpp}`：esp_random 事件/会话 id；`SelfTestPending/MarkSelfTestDone/ResetToSeed`（一次性设备 funnel 自测 + 自动回 seed）。
- `device/learning_screen/learning_screen.cc`：屏内一次性 SELFTEST 链（进屏自动跑 Start→Pause→Resume→Complete 并打每步 intent，跑完 ResetToSeed）；PersistFailed 时打 diag。
- `device/ports/nvs_outbox_storage.cpp`：Save/load 操作级日志（`save blob=… set=… commit=…` / `load peek=… len=…`）。
- 上述均可由后续模型决定保留（作为设备自检）或移除。

## 6. 独立于本问题的记录

- **官方固件基线崩溃**（非 Learning 引入）：monitor 捕获 `Guru Meditation Error (Load access fault)` 于 `esp_netif_stop_api`（Wi-Fi 停止），随后 SW reboot——官方 esp_netif 行为，与本次 defect 无关，但会打断长 monitor 会话（记录在报告 §17.2 风险）。
- 设备 USB 偶发掉线（PnP CM_PROB_PHANTOM），monitor 需在设备在线时 attach；`pnputil` 需管理员。
- IDF 构建 recipe / sdkconfig 保护 / NVS namespace `learning`（官方用 assets/wifi/audio/display… 不冲突）：见 `.workbuddy/memory/2026-09-03.md`。

## 7. 当前固件与恢复点

- `E:/b/xiaozhi.bin` = random-id 修复版（build exit=0，已上机）；设备 NVS 现为**干净 seed 态**（SELFTEST 清理后）。
- repo 远端头：见 TASK_BOARD `current_remote_head`（本 HANDOFF 提交后更新）。
- 复现步骤（拿到日志）：设备在线 → `esptool write_flash 0x200000 xiaozhi.bin` → `idf.py -p COM7 monitor` → 用户进 Learning → Start → Pause → 观察 `intent=` 与是否出现 `save blob`。
