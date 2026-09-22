# V6 M0 恢复写回（回滚）实测方案

状态：**未执行**。本文件只定义路径与判据，等待用户授权后由 Codex 排期。
责任人：Codex 执行，用户授权；WorkBuddy 不主动触碰 Flash。

## 1. 为什么必须做

`out/v6-device-private/pre-v6-full-flash.bin` 已经存在，33,554,432 字节，SHA256
`b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413`，2026-09-22 由
WorkBuddy 独立复核一致。

**但"读到并哈希过"不等于"能写回去"。** 目前 `recovery_writeback` 在
`integration/v6/m0-device-evidence.json` 中仍为 `NOT_TESTED`。含义是：

- 若当前 V6 候选固件让设备变砖、或 M1 语音联调把设备带到不可用状态，
  **没有任何一条被验证过的回退路径**；
- 一旦真出事，第一次执行完整写回就是"边试边赌"，而这正是最不该冒险的时刻。

所以这是 M0 唯一的"未验证的可逆性"缺口，优先级高于 Camera/SD/电源键集成。

## 2. 硬前提（全部满足才可执行）

| # | 前提 | 当前状态 |
|---|---|---|
| 1 | 用户对"写回 Flash / 覆盖出厂固件"的明确授权（AGENTS.md §用户最终决策） | **未取得** |
| 2 | 备份镜像 SHA256 与记录二次核对 | ✅ 已复核 `b7734369…9413` |
| 3 | Secure Boot / Flash Encryption 关闭，写回不会被熔丝策略拒绝 | ✅ 已取证 `Secure Boot: Disabled` / `Flash Encryption: Disabled` |
| 4 | 当前候选固件产物完整保留，可在写回后重刷回验证态 | ✅ `m0-candidate-04.json` + `E:/v6/s1/build/*` |
| 5 | 设备可通过 USB-Serial/JTAG 进入 download 模式 | ✅ 四轮刷写均已成功，MAC `80:f1:b2:d2:ed:14` |

环境事实（来自 `security-info.txt` / `flash-m0-04.txt`）：esptool v5.4.0、COM7、
ESP32-P4 rev v1.3、Crystal 40MHz、USB-Serial/JTAG、波特率 460800。

## 3. 两段式执行

### 段 A — 写回通路最小验证（零净变更）

思路：把**已经存在于芯片上的原值**再写一次并读回比对。写成功后设备内容与写前**逐字节相同**，
因此不需要重新刷任何东西就能回到当前状态。

```text
# 1) 先单独读回 bootloader 区（0x2000，25536 B）留作写前基线
esptool --chip esp32p4 --port COM7 --baud 460800 read_flash 0x2000 0x63C0 before-bootloader.bin

# 2) 从全片备份中切出同一区间，与步骤 1 比对（应完全一致）
#    这一步证明"备份镜像的该区间 == 芯片当前该区间"

# 3) 用备份切出的切片写回同一地址
esptool --chip esp32p4 --port COM7 --baud 460800 write_flash 0x2000 slice-bootloader.bin

# 4) 再次读回并比对
esptool --chip esp32p4 --port COM7 --baud 460800 read_flash 0x2000 0x63C0 after-bootloader.bin
```

通过判据：`sha256(before) == sha256(slice) == sha256(after)`，且设备复位后仍能
`V6M0: BOOT_READY`。

选 0x2000 而不是 0x9000 分区表的原因：bootloader 区是纯代码，写回原值不改变任何分区语义；
分区表虽然也同样"写回原值"，但历史上是风险焦点，留到段 B 一次做掉，避免两轮触碰。

> 注意：段 A 仍然是**真实的 Flash 写操作**，需要前提 1 的授权。它降低的是风险，
> 不是把"需要授权"变成"不需要授权"。

### 段 B — 全片写回（真正的回滚验证）

```text
# 1) 全片写回 32MiB
esptool --chip esp32p4 --port COM7 --baud 460800 write_flash 0x0 pre-v6-full-flash.bin

# 2) 全片读回并比对哈希
esptool --chip esp32p4 --port COM7 --baud 460800 read_flash 0x0 0x2000000 verify-full.bin
sha256sum verify-full.bin pre-v6-full-flash.bin   # 必须相等

# 3) 从串口确认写回的是原厂固件（不再是 V6 M0 诊断固件）
```

通过判据：`verify-full.bin` 与备份 SHA256 相等 **且** 设备能正常启动到原厂固件。

**段 B 的后果必须写清**：全片写回会把 `factory` 分区（0x200000）覆盖回出厂应用，
即**当前 V6 M0 候选固件会被抹掉**。要回到验证态，必须再执行一次

```text
esptool --chip esp32p4 --port COM7 --baud 460800 write_flash 0x200000 E:/v6/s1/build/xiaozhi.bin
```

写前请确认 `E:/v6/s1/build/xiaozhi.bin` 的 SHA256 仍为
`196d8718a211e660b8eac36dbb196e0e420bd3d2e4e748c142ff8fb3fc4f9d03`。

## 4. 明确不做的事

- 不写 eFuse、不烧安全启动密钥、不启 Flash 加密；
- 不改 `CONFIG_BOOTLOADER_*` 的熔丝相关项；
- 不触碰另一颗 C5 的固件（设备日志已确认 C5 为独立从机，本次范围只含 P4 的 32MiB Flash）；
- 不在 `nvs` / `nvsfactory` 上做实验性写入——写回用的是原厂原值，必须保持"逐字节还原"这一性质。

## 5. 失败处置

| 失败点 | 现象 | 处置 |
|---|---|---|
| 段 A 写后校验不符 | `read_flash` 哈希 ≠ 切片 | 停止，不改其他区域，先查 USB 供电与线缆；此时芯片内容未变（写的是原值），仍可继续用 |
| 段 A 后无法 `BOOT_READY` | 无串口输出 | 直接执行段 B 全片写回 |
| 段 B 写回中断 | esptool 超时/断连 | 重跑段 B；写入是幂等的，重复写同一镜像不会累积损坏 |
| 段 B 读回哈希不符但设备可启动 | 启动正常、哈希不同 | 说明 Flash 有坏块或写入不稳定 → 标记该设备 Flash 可靠性存疑，升级为用户级风险 |
| 段 B 后彻底无输出 | 无 boot 日志 | 串口 `--before default-reset` 重试；仍失败则设备进入不可恢复态，需人工介入（不在本方案范围） |

## 6. 完成后需要更新的文件

- `integration/v6/m0-device-evidence.json`：`recovery_writeback` → `PASS` 或 `FAIL`
- `docs/v6/V6_M0_DEVICE_REPORT.md`：回滚行改为已实测并附读回哈希
- `integration/v6/m0-hw-matrix.json`：重跑
  `python tools/v6/hw_matrix.py scan …` 让"回滚（恢复写回）"行脱离 `NOT_TESTED`

## 7. 与本方案一起提出的授权请求

需要用户对下面这一条给出**明确**答复，否则本方案不进入执行：

> 授权对 ESP32-P4（COM7，MAC `80:f1:b2:d2:ed:14`）执行上述段 A / 段 B 的 Flash 写回操作，
> 包含一次全片 32MiB 写入，写回内容为原厂备份镜像。
