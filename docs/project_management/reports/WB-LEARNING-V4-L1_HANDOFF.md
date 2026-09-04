# WB-LEARNING-V4-L1 — 真机调试问题交接（HANDOFF）

> 状态：历史故障交接，已由 §9 根因修复与 §10 真机持久化复测取代。当前限定结论为 `DEVICE_L1C_PERSISTENCE=PASS` / `CHECKPOINT_READY（验收决策归用户）`。
> 时间：2026-09-04 16:45；分支：`workbuddy/learning-v4-host-sync`；真机 COM7（Espressif USB JTAG/serial）；批次授权：仅 ota_0 application app-flash + monitor（无 erase/分区/ota_1/C5）。

> **2026-09-04 最终更新：ROOT_CAUSE_FOUND / FIX_BUILT / DEVICE_PERSISTENCE_RETEST_PASS。** Codex 在冻结头 `e9141c8` 上确认根因并提交修复 `0e53265`，整合 WorkBuddy C33 `7ee686d` 后形成 ready 代码基线 `4db2283`。主机回归、IDF build、COM7 application-only 写入和 NVS 只读取证均完成；原 `BLOCKED` 与 `DEVICE_RETEST_REQUIRED` 由 §10 取代。

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
| 事件 id 重复（outbox duplicate 保护） | ⚠️ **SUPERSEDED by §9**：`esp_random()` 本身在变化，但其 64 位结果经 nano-newlib 不支持的 `%llx` 格式化后仍产生恒定字符串；因此 duplicate 正是根因。旧日志把 `get=0`（ESP_OK 返回码）误读为计数器值，也受 `%lld` 不支持影响。 |
| draft event_id 为空（reducer 侧） | ❌ 大概率排除：reducer `makeDraft()` 用 `ctx.make_event_id ? … : EventId{}` 兜底非空；Pause/Resume/Complete 全走 makeDraft；host 同源代码 Pause 测试通过 |
| pending 容量 >200 | ❌ 排除：pending=2 |
| reducer 拒绝（状态机） | ❌ 排除：拒绝返回 intent=1（RejectedInvalidState），实际是 3（PersistFailed） |

## 4. 矛盾点（历史交接；已由 §9 消解）

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

## 7. host 复现结果（2026-09-04 17:00，C33 追加）

- 新增 `firmware/tests/unit/metalio/restart_recovery_tests.cpp`：共享 FakeDisk 模拟设备序列 seed → Start(Accepted,pending=2) → 重启(同 disk 重建 coordinator) → 校验恢复态 → Pause → Resume → Complete。
- 结果：**host 全 PASS（Pause/Resume/Complete 均 Accepted，pending 2→6）——唯一 event ID 条件下，domain/outbox/coordinator 的重启恢复路径不产生 PersistFailed**。
- 该结果把差异缩小到设备 ID 生成/适配层。§9 随后利用 nano formatter 的真机日志旁证定位到 `learning_runtime` 的 `%llx` ID 格式化，并给 outbox 增加批内重复防线；因此无需继续怀疑 NVS 覆盖写或句柄时序。

## 8. 当前固件与恢复点

- 本节是故障现场的历史恢复点，已由 §10 **SUPERSEDED**。
- 当时 `E:/b/xiaozhi.bin` = random-id 修复版（build exit=0）；“设备 NVS 为干净 seed 态”不再代表当前状态。
- repo 远端头：见 TASK_BOARD `current_remote_head`（本 HANDOFF 提交后更新）。
- 复现步骤（拿到日志）：设备在线 → `esptool write_flash 0x200000 xiaozhi.bin` → `idf.py -p COM7 monitor` → 用户进 Learning → Start → Pause → 观察 `intent=` 与是否出现 `save blob`。

## 9. 根因与修复候选（2026-09-04 17:10）

### 9.1 根因证据链

1. 固件配置明确为 `CONFIG_NEWLIB_NANO_FORMAT=y`。
2. 既有真机日志把 `%zu` 输出为 `zu`、把 `%lld` 输出为 `ld`，直接证明该构建的 nano formatter 不支持这些长度修饰符。
3. `NextEventId/NextSessionId` 虽改用 `esp_random()`，却仍用 `snprintf("%llx", random64)` 生成字符串；不同随机数因此落成相同的文字 ID。
4. Start 同批产生两个 draft，而旧 outbox 只检查 draft 与**已有 pending** 的重复，不检查**同批 draft 之间**的重复，所以 Start 能提交两个相同 event_id；Pause/Complete 的恒定 ID 随后命中 existing duplicate，在 `storage.commit()` 前返回 `InvalidTransition`。这与“Start 成功、后续失败、无 Save、pending=2”的全部证据一致。

### 9.2 修复

- `0e53265`：新增无 printf 的固定 16 位十六进制编码 `formatEntropyId()`，直接格式化两次 `esp_random()` 的 32-bit 输出；同时消除 signed 64-bit 左移风险。
- outbox 新增同一 transition 内 event_id 重复保护，禁止再次写入“同批双重复”的毒化队列。
- 设备诊断日志移除 `%zu/%lld`，避免 nano formatter 再次制造错误证据。
- 修正 SELFTEST 完成标志顺序：`ResetToSeed()` 成功后才写 `stest=1`，避免 `nvs_erase_all()` 把标志立即删掉并在下次进屏重复运行自动链。
- 新增回归：确定性 entropy ID 格式、同批重复拒绝、codec 持久化 → 重启 → 旧 pending → Pause 成功。

### 9.3 验证与当前恢复点

- 主机：整合 C33 后 11/11 单测二进制 PASS；关键套件 restart_recovery PASS、LearningApp 5/5、outbox 22/22、codec PASS；接口 cross-check exit=0。
- 设备构建：`idf.py -C E:/c -B E:/b build` exit=0；新 `xiaozhi.bin` **9,175,856 B**；SHA-256 `6d27653a7baa690bdb63e7288a27a5b2b5ad0b347ef5f1b9354fe84b5722aa1f`；`ota_0` 可容纳，`ota_1` 继续保持禁止/不触碰。
- 构建镜像与 repo 权威的四个修改文件 + 新 header 已逐文件比对一致。
- 本条“尚未 app-flash / 待复测”为历史状态，已由 §10 **SUPERSEDED**。

## 10. Codex 真机持久化复测（2026-09-04）

### 10.1 写入对象与范围

- 分支/代码基线：`codex/wb-learning-v4-l1-ready` @ `4db2283086971d4be11272b5fd99f347d1796b98`。
- COM7：ESP32-P4 rev v1.3，USB-Serial/JTAG，MAC `80:f1:b2:d2:ed:14`。
- `xiaozhi.bin`：9,175,856 B；SHA-256 `6d27653a7baa690bdb63e7288a27a5b2b5ad0b347ef5f1b9354fe84b5722aa1f`。
- 仅执行 `write_flash 0x200000 E:/b/xiaozhi.bin`；esptool `Hash of data verified.`。未触碰 erase、bootloader、partition、ota_1、C5、eFuse、Secure Boot 或 Flash Encryption。

### 10.2 用户操作与 NVS 法证证据

用户完成屏侧测试并反馈“测试完成了”。物理交互期间 monitor 未保持连接，所以不能声称每一步均有 `intent=Accepted` 串口证据，也不能把 `stest=1` 单独写成本次 SELFTEST 日志 PASS。

测试后只读导出 NVS `0x3c000 + 0xD2000`：860,160 B，SHA-256 `3263c3df864b53fb1d47ec75b208ae50ae3c355787516ee847eb39ee310a0cda`。原始整分区留在仓库外，仅披露 `learning` namespace 的脱敏结果：

- NVS 有效页 CRC32 正常；`learning/st` blob 可解码，magic=`C4L1OUTBOX`；`learning/stest=1` 标志存在。
- 结束态 `S|0`，无活动 session；`E|20` 为当前待上送 outbox，不定性为清理缺陷；`Q|21`、`A|0`、`F|0`、`R|0`。
- sequence 1..20 连续无缺口；20 个 ID 均匹配 `ev-[0-9a-f]{16}`、全部唯一、无 duplicate。
- 事件类型：TaskStarted 2、TaskPaused 6、TaskResumed 6、TaskCompleted 2、StudySessionStarted 2、StudySessionCompleted 2。
- 上述事件可能混合自动链与人工链，不声称“恰好两条人工链”；但它们直接证明 Start/Pause/Resume/Complete transition 已成功 commit，且在 USB/JTAG 复位后仍可读回。

### 10.3 结论、限制与独立风险

- 限定结论：**`DEVICE_L1C_PERSISTENCE=PASS` / `CHECKPOINT_READY（验收决策归用户）`**。原恒定 event ID/duplicate 症状未复现。
- 原任务包要求“monitor 日志级 PASS + 用户屏侧确认”；本轮用户确认已具备，但逐步 monitor 证据未采集完整。NVS 法证足以支持持久化能力通过，不将结论扩大为整个 L1、联网、发布或整机稳定性 PASS。
- 后续进入页面时观察到 GT911/TCA95xx/BQ27220 短暂 I2C timeout，重置后曾恢复；单列 `HARDWARE_VERIFY_REQUIRED`，不归因于本次 persistence 修复。
- 蜂窝注册超时与 OTA DNS/TLS 无网失败超出本地 L1c 范围。
- 调试时曾在构建树准备临时自动进入 Learning 钩子；用户反馈测试完成后该钩子即被移除、重新构建并恢复权威镜像哈希，且临时版本从未写入设备。

完整审计见 `CODEX_WB_LEARNING_V4_L1_DEVICE_TEST_2026-09-04.md`。后续按用户最新要求改为 App-first 批量开发，不为单个功能反复刷机或持续读取串口。
