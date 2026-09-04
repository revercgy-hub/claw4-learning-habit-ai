# CODEX — WB-LEARNING-V4-L1c 真机持久化复测报告

> 结论：`DEVICE_L1C_PERSISTENCE=PASS`，任务可进入 `CHECKPOINT_READY（验收决策归用户）`。本结论仅覆盖 Learning 本地状态机、NVS 持久化与重启后读回，不代表网络、语音、MCP、AI 或整机长期稳定性通过。

## 1. 测试对象与安全边界

| 项 | 证据 |
| --- | --- |
| Repo 分支 | `codex/wb-learning-v4-l1-ready` |
| 代码基线 | `4db2283086971d4be11272b5fd99f347d1796b98` |
| 设备 | COM7，ESP32-P4 rev v1.3，USB-Serial/JTAG，MAC `80:f1:b2:d2:ed:14` |
| 应用镜像 | `xiaozhi.bin`，9,175,856 B |
| SHA-256 | `6d27653a7baa690bdb63e7288a27a5b2b5ad0b347ef5f1b9354fe84b5722aa1f` |
| 允许范围 | 仅 `ota_0` application 地址 `0x200000` 写入、启动观察与只读证据采集 |
| 未触碰 | `erase_flash`、bootloader、partition、`ota_1`、C5、eFuse、Secure Boot、Flash Encryption |

写入命令：

```text
python -m esptool --chip esp32p4 -p COM7 -b 460800 --before default_reset --after hard_reset write_flash 0x200000 E:/b/xiaozhi.bin
```

esptool 报告写入 9,175,856 B，并给出 `Hash of data verified.`。启动日志确认从 `ota_0` 运行，应用版本 2.0.51、ESP-IDF 5.5.4。

## 2. 用户操作与取证方式

用户在设备屏幕上完成本轮操作并反馈“测试完成了”。物理操作期间 monitor 未保持连接，因此本报告不宣称逐次触摸均捕获了 `intent=Accepted`，也不把 `stest=1` 单独解释为本轮自动 SELFTEST 的串口证据。

为避免再次写 Flash，随后只读导出 NVS 分区：

```text
python -m esptool --chip esp32p4 -p COM7 read_flash 0x3c000 0xD2000 E:/workbuddy/claw4-l1-evidence/nvs_after_l1_test.bin
```

- 原始导出文件位于仓库外，不提交 Git；其 SHA-256 为 `3263c3df864b53fb1d47ec75b208ae50ae3c355787516ee847eb39ee310a0cda`。
- 仅解析 `learning` namespace；不在报告中展开其他 namespace，避免泄露设备配置。
- NVS 已用 ESP-IDF 工具校验：有效页 CRC32 正常，其余页为空。

## 3. `learning` namespace 结果

| 检查项 | 结果 |
| --- | --- |
| `learning/stest` | 值为 `1`；仅证明完成标志存在 |
| `learning/st` | blob 存在且可解码，magic=`C4L1OUTBOX` |
| 任务 | 2 个 demo task |
| 活动会话 | `S|0`，测试结束时无活动 session |
| Pending outbox | `E|20`；保留为未上送事件事实，不定性为清理缺陷 |
| Sequence | 1..20 连续，无缺口 |
| Event ID | 20 个均匹配 `ev-[0-9a-f]{16}`，全部唯一，无 duplicate |
| 计数器 | `Q|21`、`A|0`、`F|0`、`R|0` |

事件类型统计：

| 类型 | 数量 |
| --- | ---: |
| TaskStarted | 2 |
| TaskPaused | 6 |
| TaskResumed | 6 |
| TaskCompleted | 2 |
| StudySessionStarted | 2 |
| StudySessionCompleted | 2 |

以上事件可能混合自动链与人工链，因此不把它们表述为“恰好两条人工操作链”。但它们足以证明 Start/Pause/Resume/Complete 相关 transition 均已写入持久化 outbox，且修复后的 entropy ID 没有复现恒定 ID/duplicate 问题。该状态在 USB/JTAG 复位后仍可只读取回，满足本轮重启持久化验证目标。

## 4. 判定与限制

`DEVICE_L1C_PERSISTENCE=PASS` 的依据是：

1. 权威候选镜像已按 app-only 边界写入且哈希校验通过；
2. 用户完成屏侧测试；
3. NVS 中存在连续、可解码、ID 全唯一的 Start/Pause/Resume/Complete 与 StudySession 事件；
4. 测试结束无活动 session，且重置后仍能读回同一持久状态；
5. 原始缺陷所需的恒定 event ID/duplicate 条件未复现。

保留限制与独立风险：

- 物理操作时未持续采集 monitor，不能声称每一步都有串口级 `Accepted` 证据；本轮采用用户确认 + NVS 法证证据收口。
- 后续重新进入页面时出现过短暂 GT911/TCA95xx/BQ27220 I2C timeout，重置后曾恢复；该项单列 `HARDWARE_VERIFY_REQUIRED`，不属于 L1c 本地持久化缺陷。
- 蜂窝注册超时及 OTA DNS/TLS 失败发生在无网络现场，超出 L1c 本地持久化范围。
- 调试过程中曾在构建树准备临时自动进入 Learning 的钩子，但在用户反馈测试完成后立即移除、重新构建并恢复上述原始镜像哈希；该临时版本从未写入设备。

## 5. 后续节奏

按用户 2026-09-04 最新决定，后续不再为小功能逐项上机。先在 App/Host 模式完成一个完整业务批次并跑自动门禁，阶段末由用户按固定验收单执行一次真机测试；只有屏幕诊断信息无法解释失败时才连接串口。详见 `docs/project_management/tasks/CODEX-APP-FIRST-001_MVP_FULL_LOOP.md`。
