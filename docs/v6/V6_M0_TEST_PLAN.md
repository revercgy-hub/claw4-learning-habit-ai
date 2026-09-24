# V6 后续测试计划

> **SUPERSEDED 历史计划，不是当前执行指令。** 本文最初适用于 Candidate04（`m0-candidate-04.json`，app SHA256 前缀 `196d8718a211e660…`）；候选、工具、职责、M1 范围和设备授权均已变化。不得按本文旧串口/刷写步骤操作。当前队列、证据规则及授权以 [V6_TASK_BOARD](V6_TASK_BOARD.md)、[V6_ARCHITECTURE](V6_ARCHITECTURE.md) 和根 [AGENTS.md](../../AGENTS.md) 接管节为准。保留本文供历史取证。

Candidate04 相关结果不得代表当前 Candidate19。当前实施状态见 [V6_TASK_BOARD](V6_TASK_BOARD.md)：第一实施波已整合至 `7263f67`，L-02 定向修复已整合至 `a128341`；独立 R-01 在 `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` 复审 PASS，Host 96/0；S-03 为 IN_PROGRESS 且从该 SHA 由唯一设备 owner 执行。Candidate20 尚未构建，待 Sol 核验 ID 唯一性；Candidate19 仍为 built, not flashed，设备结果不转移。A-02 为 QUEUED，M0 为 IN_PROGRESS，M1 为 BACKLOG（NAS 连续语音 20 轮）。本文旧“设备→主机 relay→后端”的学习闭环属于旧阶段，不是 M1 的验收结果或范围。

---

## 0. 先定规矩（这几条是本次审计踩出来的，破一条结论就不可信）

| # | 规矩 | 反面教材（本题都真实发生过） |
| --- | --- | --- |
| 1 | **报"从未观察到 X"之前，先证明 X 的观测口有效**，并附源码 `file:line` | `wake=` 差点被读成"检测到唤醒"，实际是反过来 |
| 2 | **一条证据只证一件事**；缺信号一律 `NOT_VERIFIED`，绝不由相邻项推断 | "热点能起来"曾被当作"网络通了" |
| 3 | **日志字符串不等于事实**，去读打印它的那行源码 | `No AP found` 实际是"无匹配 SSID"，不是"扫不到 AP" |
| 4 | **取证窗口必须大于上游超时常量** | `CONNECT_TIMEOUT_SEC=60`，而取证上界正好 60 s → 永远看不到配网 |
| 5 | **跨捕获按最大值合并**，新日志不得抹掉旧证据 | 零点击的网络探针曾把触摸证据抹成 0 |
| 6 | **关键事实两侧独立复核**，不采信报告文字 | 备份/产物哈希逐个重算；NVS 与主机勘测互证才敢下隐藏 SSID 的结论 |
| 7 | **沙箱内 curl / python socket / ping 都不能判断局域网可达性** | curl 走 `127.0.0.1:<随机端口>` 代理；socket 返回 `EACCES(10013)`；热点屏蔽 ICMP |
| 8 | **给工具传路径一律写绝对路径，不做 `../` 拼接** | 一次 JSON 输出落到被忽略目录，md 与 json 短暂不同步 |

---

## 1. 现在就能测（不改代码、不刷机）

### 1.1 唤醒词 —— 最后一项 `NOT_VERIFIED`

**刺激**（二选一，报告里必须写明用的哪种）：
- **(a) 真人**：对着设备清晰说「你好小智」
- **(b) 播放录音**：手机在设备旁约 30 cm 播放「你好小智」语音，音量中等偏大
  → 只能证明 mic→AFE→WakeNet 链路，**不等于真人唤醒**，报告须标注

**执行**（一次 105 秒；`capture_device.py` 上界 60 s，所以分两段连跑）：

```bash
PY=.../idf5.5_py3.12_env/Scripts/python.exe
B=E:/workbuddy/claw4-v6-wb/out/v6-device-private
$PY tools/v6/capture_device.py --port COM7 --seconds 45 --output "$B/wakeword-m0-01.txt"
$PY tools/v6/capture_device.py --port COM7 --seconds 60 --output "$B/wakeword-m0-02.txt"
```

**时序**：设备已联网且唤醒引擎 armed，**无需复位即可直接说**。
在 0–15 s、30–45 s、60–75 s、90–105 s 各说一次，共 4 次，中间保持安静。

**判据**：

| 观察 | 结论 |
| --- | --- |
| 出现 `V6M0: WAKE_DETECTED (local only)` ≥ 1 次 | 唤醒词 → **PASS** |
| 该次之后某条 HEALTH 出现 `wake=0` | 旁证：锁存位被清（`audio_service.cc:97`） |
| 4 次全无 `WAKE_DETECTED` 且 `wake` 始终为 1 | 记 `FAIL` 或 `INCONCLUSIVE`（**视刺激方式**：(a) 记 FAIL，(b) 记 INCONCLUSIVE 并改日真人复测） |

**⚠️ 以下是 Candidate04 历史操作说明。USB reset 只触发 USB/串口复位，不等于断电 cold boot；不能据此填写冷启动证据。** M0 诊断脚手架有个自恢复缺陷：
唤醒回调会清掉 `WAKE_WORD_RUNNING` 位（`audio_service.cc:97`），而音频输入任务
**只在位被置位时才把音频喂给引擎**（`audio_service.cc` 的 `AudioInputTask`），
空闲循环又不会重新置位 ⇒ **一次检测之后，唤醒检测永久失效**，直到按屏幕按钮或重启。
所以：`wake` 一开始就是 0 的那一轮，**测的是"检测器已停机"，不是唤醒率**。

**不做**：不设"命中率门槛"。真人 vs 录音、距离、口音差异太大，定阈值是伪精确。
命中次数照实记录，作为背景信息。

**不把响指当替代刺激**：响指不在 WakeNet 的目标可分空间内，不可靠。
但它**并非绝对不触发**——2026-09-22 的锁存证据指向响指造成过一次误触发。
**误触发是可用性噪音，不是功能可用性证据**，两者不能混。

### 1.2 长稳 soak —— 补掉矩阵里"long-run stress not exercised"
矩阵当前只测到 **110 秒**，`Flash / PSRAM` 行的备注明写"长期压力未测"。这条没人排过期，但它是唯一
能在**不刷机**的前提下暴露内存泄漏 / 看门狗 / 协议栈退化的手段。

**执行**：`capture_device.py` 循环 30 min（30 段 × 60 s，连续跑），设备保持联网空闲。

```bash
for i in $(seq -w 1 30); do
  $PY tools/v6/capture_device.py --port COM7 --seconds 60 \
      --output "$B/soak-m0-$i.txt"
done
```

**判据**：

| 检查 | 通过条件 |
| --- | --- |
| `V6M0: HEALTH` 的 `free` / `psram` | 30 min 内**无单调下降趋势**（允许 ±64 KB 抖动） |
| `assert failed` / `Guru Meditation` / `abort()` | 计数 = 0 |
| 重启痕迹（`rst:0x…`、`ESP-ROM:` 再次出现） | 无 |
| `task_wdt` 触发 | 无 |
| WiFi 断连（`WifiBoard: WiFi disconnected`） | 计数 = 0，或 ≤1 且能自动重连 |

**这一条我可以自己跑，不需要你配合**（30 分钟无人值守）。

### 1.3 历史启动可靠性记录（非当前执行步骤）

**为什么必须有这一条**：2026-09-22 查出一条真实缺陷 —— `claw4_board.cc:92` 的
`ESP_ERROR_CHECK(i2c_master_probe(bus_, 0x20, 100))` 把**间歇性** I2C 探测超时升级成
致命 abort，实测到 **3 次启动尝试 / 2 次 abort / 1 次成功**。
而在此之前，连续 10 份捕获都是一次启动成功 —— **"几次没崩"完全不能证明启动可靠**。

**历史执行草案**：旧方案使用串口 `--reset`，这只是 USB reset，不能证明断电 cold boot。本文不提供当前 cold boot 操作授权。

```bash
for i in $(seq -w 1 20); do
  $PY tools/v6/capture_device.py --port COM7 --seconds 20 --reset \
      --output "$B/bootcount-m0-$i.txt"
done
```

> ⚠️ 若某轮起不来，设备可能卡在 `SW_CPU_RESET` 循环里；**不要立即重跑**，
> 先保存该轮日志（它就是证据），必要时断电再继续。

**判据**：

| 检查 | 通过条件 |
| --- | --- |
| `Calling app_main()` 出现次数 == 轮次 | 每轮只启动一次，无内部重启 |
| `abort() was called` / `rst:0x.. (SW_CPU_RESET)` | **计数 = 0** |
| `V6M0: BOOT_READY` | 每轮都出现 |
| 失败率 | **0/20 才算通过**；任何 1 次失败都要先修 `claw4_board.cc:92` 再复测 |

串口日志用 `hw_matrix.py` 直接给出这四项（`boot attempts` / `aborts` / `panic resets` / `BOOT_READY`）。

---

## 2. 需要你明文授权才能做（写 Flash）

按 `V6_M0_RECOVERY_WRITEBACK_PLAN.md` 执行：

| 段 | 内容 | 风险 |
| --- | --- | --- |
| A | 把**原值**写回 `0x2000` bootloader 区并读回比对（零净变更） | 低；写的是芯片上已有的内容 |
| B | 全片 32 MiB 写回（真正验证回滚） | **会抹掉当前 V6 候选固件**，之后需重刷回验证态 |

授权语句（需要你原样确认）：

> 授权对 ESP32-P4（COM7，MAC `80:f1:b2:d2:ed:14`）执行段 A / 段 B 的 Flash 写回操作，
> 包含一次全片 32 MiB 写入，写回内容为原厂备份镜像。

---

## 3. 等 Codex 交付固件才能测

| 项 | 依赖 | 说明 |
| --- | --- | --- |
| SD 卡 / Camera / 电源键 | M0-2 剩余固件 | 当前候选未集成，矩阵记 `NOT_TESTED` |
| **旧阶段学习 relay 闭环** | 旧阶段固件 | 历史记录，不是当前 M1 验收项 |

### 3.1 旧 relay 学习闭环记录（SUPERSEDED，不是 M1）

```bash
/…/学习习惯培育AI/backend/.venv/Scripts/python.exe -m pytest -q      # 于 backend/
# → 79 passed, 1 warning in 6.99s（含 tests/e2e/test_host_loop.py）
```

该历史 e2e 覆盖的是旧阶段链路：家长建今日任务 → 设备注册/签权/拉任务 → 提交离线事件序列 →
**一次丢响应后重发同一批 event_id → 去重 + ACK 收敛** → 家长看板恰好一条完成记录 → 家庭隔离。

该历史 e2e 结果仅说明当时主机学习 relay 闭环测试通过；它不能证明当前 NAS 连续语音 20 轮，也不能排除 M1 的 NAS、设备或网络问题。

### 3.2 旧阶段 relay 真机前置（SUPERSEDED，不适用于当前 M1）

| 前置 | 状态 |
| --- | --- |
| 设备开机自动联网 | ✅ `NETWORK_IP=PASS` |
| 主机与设备同一 L2 | ✅ 同 SSID/同 AP BSSID/同 `/24`，ARP 双向解析，无 VPN 劫持 |
| 设备能主动连到主机 relay | ❌ 旧阶段待办；不定义当前 M1 |
| 隐藏 SSID 重连缺口 | ❌ 仍在（`V6_M0_NETWORK_PROBE.md` §8.6）：换回不广播的 AP 会复现 |

---

## 4. 每次测试的标准动作（照着做，别跳）

```bash
# 1) 确认设备在线且没被别人占串口
#    （若上一步失败，先杀遗留捕获进程，再等几秒让 USB CDC 重枚举）
$PY tools/v6/capture_device.py --port COM7 --seconds <N> --output <绝对路径>

# 2) 解析 + 汇总（含本次新增捕获）
$PY tools/v6/hw_matrix.py scan \
  --boot <全部历史捕获>… --boot <本次捕获> \
  --flash <最近一次刷写日志> \
  --candidate integration/v6/m0-candidate-04.json \
  --json-out "<绝对路径>/integration/v6/m0-hw-matrix.json" \
  --md-out   "<绝对路径>/docs/v6/V6_M0_HW_MATRIX.md"

# 3) 必要时的只读独立复核
$PY -m esptool --chip esp32p4 -p COM7 -b 460800 read_flash <off> <len> <绝对路径>
sha256sum <产物>            # 与 m0-candidate-04.json 逐个比对
$PY -m unittest discover -s tools/v6 -t tools/v6 -p "test_*.py"   # 应 38 passed

# 4) 更新报告 → 提交（一笔只解决一件事）→ 交用户 push
```

**判定权**：矩阵里任何一行转 `PASS` 都必须能指到一条具体日志行或一次用户确认；
`hw_matrix.py` 已把这条编码成不变量（缺信号 = `NOT_VERIFIED`；音频回环不得提升唤醒词；
软 AP 不得提升为关联成功）。

---

## 5. 未闭合项与责任

| 项 | 状态 | 谁 | 需要什么 |
| --- | --- | --- | --- |
| 唤醒词 | `NOT_VERIFIED` | 用户 + 我 | 一次真人（或录音）刺激 |
| 回滚（恢复写回） | `NOT_TESTED` | 用户授权 + Codex 执行 | 明文授权 |
| 长稳 soak | 未排期 | **我可以自己跑** | 只占 30 分钟 |
| SD / Camera / 电源键 | `NOT_TESTED` | Codex | M0-2 固件 |
| M1：NAS 连续语音 20 轮 | BACKLOG | M0 PASS 后再规划 | 本页 Candidate04 旧 relay 流程不能代替 M1 |
| 隐藏 SSID 重连缺口 | 未修 | Codex | 扫描 N 轮无匹配后回退直连（§12.5 方案 A） |
| HEALTH 交互期丢采样 | 未修 | Codex | `ticks % 10` 改按时间判定 |
| `mac type is incorrect` 时序 | 未修 | Codex | MAC 读取延后到 `Coprocessor Boot-up` 之后 |

---

## 6. 建议 Codex 复检重点

1. §0 的 8 条规矩是否接受为**本阶段取证标准**（后续所有真机结论按此受检）；
2. §1.1 唤醒词的判据与"不设命中率门槛"是否同意；
3. §1.2 soak 的通过条件（抖动容差 ±64 KB、断连 ≤1 次）是否合理；
4. （SUPERSEDED）旧 relay 测试数字和范围是否可作为历史参考；不得用于当前 M1 判定；
5. §5 里排给 Codex 的三项修复（隐藏 SSID 回退、HEALTH 采样、MAC 时序）的优先级是否调整。
