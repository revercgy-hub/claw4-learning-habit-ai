# WB-V6-M0 任务交接报告（→ Codex）

任务 ID：`WB-V6-M0-AUDIT-001`
交接日期：2026-09-22
分支：`workbuddy-v6-m0-closure-tools`（基 `codex/v6-foundation` @ `1b52240`）
提交：`6340ef3`（本地 HEAD；远端 `92e9452`，最新一笔待用户 push）
盘点对象：`m0-candidate-04.json`，应用 SHA256 `196d8718a211e660…`，设备 ESP32-P4 rev1.3 @ COM7

---

## 0. 一句话状态

**M0 未通过，且新查出一个启动可靠性缺陷。** 12 项矩阵：**6 PASS / 1 FAIL / 3 NOT_VERIFIED / 2 NOT_TESTED**。
另外交付了一套可复现的取证工具，并给出两份可直接应用的补丁/方案。

---

## 1. 最重要的两件事（请优先看这里）

### 1.1 🔴 新缺陷：I2C 探测超时被 `ESP_ERROR_CHECK` 升级为致命 abort → 启动循环

`out/v6-device-private/netprobe-m0-07.txt` 记录到 **3 次启动尝试、2 次 abort、1 次成功**：

```text
rst:0x17 (CHIP_USB_UART_RESET)          ← 复位
I (1531) main_task: Calling app_main()
E (1770) i2c.master: I2C transaction timeout detected
E (1771) i2c.master: probe device timeout. Please check if xfer_timeout_ms and pull-ups ...
ESP_ERROR_CHECK failed: esp_err_t 0x107 (ESP_ERR_TIMEOUT) at 0x4803fb08
file: "./main/boards/metalio/claw4-learning-v6/claw4_board.cc" line 92
func: void Claw4Board::InitializeBus()
expression: i2c_master_probe(bus_, 0x20, 100)
abort() was called at PC 0x4ff1ed41 on core 0
  ...
rst:0xc (SW_CPU_RESET)                  ← 第 2 次
E (1818) i2c.master: I2C bus is still busy but software timeout detected
abort() was called at PC 0x4ff1ed41 on core 0
rst:0xc (SW_CPU_RESET)                  ← 第 3 次
I (1725) Claw4V6: I2C/TCA9555 initialized   ← 这次才成功
V6M0: BOOT_READY IDF=v6.1
```

**性质**：`claw4_board.cc:92` 对 **TCA9555（地址 0x20）** 的 `i2c_master_probe` 是**间歇性**的
（其余 10 份捕获全部一次成功），但 `ESP_ERROR_CHECK` 把一次偶发探测超时变成了**不可恢复的 abort**。
第二次失败还带 `I2C bus is still busy` —— 上一次崩溃留下的**总线锁死残留**。

**为什么这条必须报**：对一个给孩子用的设备，"开机偶尔起不来、并且会反复重启"是不可接受的行为。
**建议**：探测失败改为有限次退避重试 + 失败时进入可诊断状态（而不是 abort），
并考虑崩溃前对 I2C 总线做一次恢复（`i2c_master_bus_reset` 之类），避免第二次必挂。

### 1.2 🔴 唤醒词：已排除全部外围，卡在"没有电平探针"

用户配合实测：复位后 105 s、**约 10 次**准确说「你好小智」、**正对设备**，`WAKE_DETECTED` = **0**。

**已排除**（有证据，请勿重复验证）：

| 排除项 | 依据 |
| --- | --- |
| 唤醒词模型错配（V5.3 那个坑） | 回读设备 `0x10f000` 前 291,042 B，SHA256 `7c87dd7a…` **与 `build/srmodels/srmodels.bin` 逐字节相同**；内含 `wn9_nihaoxiaozhi_tts` + 字面量 `你好小智` |
| 检测器未启动 | 复位后 `wake=1` 全程成立（`IsWakeWordRunning()`） |
| 麦克风不工作 | 3 s 采集 + 回放可闻；而该回环依赖引擎 `OnOutput()`，**引擎必须先被 feed 才可能有输出** |
| 音频通道整体接反 | 同上 |
| 刺激条件不足 | 用户已确认次数/内容/朝向 |

**剩余唯一未知**：**到达 WakeNet 的信号电平是否足够触发阈值**。回环可闻只证明"有信号"
（回环是直通放大），阈值判定是另一回事，而当前**没有任何 VAD/电平可观测点**。

**关键抓手**：`AudioServiceCallbacks::on_vad_change` **已存在**（`audio_service.h:85`），
上游 `application.cc:84` 已经接了它，**只有 M0 脚手架 `m0_diagnostics.cc` 没接**。
补上它就是判定的关键探针 → 见 `docs/v6/V6_M0_WAKEWORD_INSTRUMENTATION_PROPOSAL.md`（含可直接应用的补丁）。

### 1.3 🟠 顺带查出的脚手架缺陷：一次检测后唤醒检测永久停机

`audio_service.cc` 的 `AudioInputTask` **只在 `AS_EVENT_WAKE_WORD_RUNNING` 置位时才 `Feed()`**，
而唤醒回调**在命中时清掉该位**（`:97`），`m0_diagnostics.cc` 的空闲循环又不会重新置位。
⇒ **一次命中之后，唤醒检测永久失效**，直到按屏幕按钮或重启。
⇒ 不修这个，任何"唤醒率"测试都只能测到第一次。补丁已含在提案的 P2。

### 1.4 🟠 我此前报告里有一个结论需要标 SUPERSEDED

`WB-V6-M0-AUDIT_2026-09-22.md` §0/§1 曾写 **"启动 PASS，assertions=0"**。
那是基于**当时已有的捕获**；`netprobe-m0-07.txt` 加入后，同一套判定立刻把该行改成 **FAIL**。
**以本文档 §1.1 与 `V6_M0_HW_MATRIX.md` 为准**，旧结论标记 `SUPERSEDED`（已在审计报告 §15 标注）。
**这也说明：M0 的"启动通过"此前只是"这几轮刚好没崩"，不是稳定性结论。**

---

## 2. 矩阵现状（`docs/v6/V6_M0_HW_MATRIX.md`，自动生成）

| 项 | 状态 |
| --- | --- |
| Flash / PSRAM | PASS（本次仅 1 次刷写校验；**长期压力未测**） |
| **启动** | **FAIL**（boot attempts=3, BOOT_READY=1, aborts=2, panic resets=2） |
| 显示 | PASS（原生 RGB888 + 用户确认可见） |
| 触摸 | PASS（3 轮点击 → 采集周期） |
| 音频 | PASS（回环用户确认；音质/音量/AEC 未量化） |
| **唤醒词** | **NOT_VERIFIED**（见 §1.2） |
| C5 链路 | PASS（SDIO + 从机枚举 + 从机启动） |
| 配网模式 | PASS（热点 `Xiaozhi-79D9`、DHCP、配置页） |
| 网络 / IP | PASS（`Connected to WiFi` + `sta ip` 取得） |
| 资源分区 | PASS（`Assets applied=1`） |
| SD / Camera / 电源键 | NOT_TESTED（未集成） |
| **回滚（恢复写回）** | **NOT_TESTED**（备份可读可哈希，**从未写回**；回滚不可信） |

判定口径已编码进工具的不变量：缺信号 = `NOT_VERIFIED`；捕获到 abort = `FAIL`；
**音频回环不得提升唤醒词**；**软 AP 不得提升为关联成功**。

---

## 3. 独立复核结果（对你此前报告的核对）

| 你报告的声称 | 我的复核 | 结论 |
| --- | --- | --- |
| 完整备份 33,554,432 B / SHA256 `b7734369…9413` | 实测一致 | ✅ |
| m0-candidate-04 各产物哈希 | 对 `E:/v6/s1/build/*` 逐个重算，6 项全等 | ✅ |
| 12 项工具测试 PASS | 重跑，12/12 OK | ✅ |
| 显示已由用户确认 | 日志 + 用户确认 | ✅ |
| 触摸事件链通过 | `TOUCH count=1/4/5` → record → playback queued | ✅ |
| 应用 3,043,216 字节 | 实测一致 | ✅ |
| 「C5 枚举 chip ID=12，扫描可运行」 | 枚举属实，但**"扫描可运行"表述过强**：扫到的是"无匹配已保存 SSID" | ⚠️ 已在探针报告 §2 更正 |
| 音频待验收 | 已升级为"用户确认回环成功"，但**不得推出音质/AEC/唤醒率** | ⚠️ 已在探针报告 §4 限定范围 |

---

## 4. 另外两个需要你决策的技术结论

### 4.1 网络重连的隐藏 SSID 缺口（未修，只被绕过）

`wifi_station.cc` 的 `StartConnect()` 只有两个调用点（`HandleScanResult()` / `WifiEventHandler`），
**没有"按已保存 SSID 直连"的入口**，而匹配用 `strcmp(ap_record.ssid, item.ssid)`，
隐藏 SSID 的 `ap_record.ssid` 是空串 ⇒ **对不广播 SSID 的 AP 永远连不上**，
60 s 后回落配网，无限循环。配网路径（`WifiConfigurationAp::TryConnect`）把凭据直接写进
`wifi_config.sta`，不依赖扫描，所以"配网连得上、重连连不上"。

**反证实验已做**：换一个**会广播 SSID** 的 AP、同一固件、同一频道 11 → **12.5 s 关联成功并取得 IP**。
⇒ 该结论已从推论升级为证实。**当前只是绕过，缺口仍在**（用户换回隐藏 AP 会复现）。

建议：方案 A —— 扫描连续 N 轮无匹配后回退为"用 `SsidManager` 第一条已保存凭据直连"（照抄配网的成功路径）。
注意 `78__esp-wifi-connect` 是 **managed component**，改动须记录并保留上游比对基线。

### 4.2 硬件嫌疑：`claw4_audio.cc:70` 的增益换算（**未改，且建议先量再改**）

```cpp
dest[total + i] = static_cast<int16_t>(std::clamp<int32_t>(buffer[i] >> 12, -32768, 32767));
// Preserve the source board's ADC alignment/gain, verify both channels on hardware.
```

**这行你自己注释了"需要在硬件上验证"，该前置条件至今未兑现。**
算术上的两种可能：24bit **左对齐**在 32bit 槽则正确应为 `>> 16`（现在 `>> 12` 过冲 16 倍）；
24bit **右对齐**则只到 int16 满量程约 6%（≈ −24 dB，**可能就低于 WakeNet 阈值**）。
**在拿到 rms 数据之前，我不建议改这个系数** —— 改 gain 而不先量电平是拿猜替换猜。
提案里给了可选的 P4：每秒打印一次 int16 的 rms/peak，最快定论。

---

## 5. 需要你做的（按优先级）

| # | 事项 | 类型 | 相关文档 |
| --- | --- | --- | --- |
| 1 | 评估并修复 **I2C 探测失败 → abort → 启动循环**（`claw4_board.cc:92`） | 代码 | 本文 §1.1 |
| 2 | 采纳唤醒词插桩补丁（P1 `on_vad_change` / P2 重新 arm / P3 计数，可选 P4 rms），重新构建 + 冻结候选 + 刷机 | 代码 | `V6_M0_WAKEWORD_INSTRUMENTATION_PROPOSAL.md` |
| 3 | 修 `wifi_station` 的隐藏 SSID 重连缺口（或确认上游新版已修） | 代码 | `V6_M0_NETWORK_PROBE.md` §8.6 / §12.5 |
| 4 | 决策是否动 `claw4_audio.cc:70` 的 gain —— **但必须先有 rms 数据** | 决策 | 本文 §4.2 |
| 5 | 是否排期**长稳 soak**（30 min，我可以自己跑，不需要用户） | 调度 | `V6_M0_TEST_PLAN.md` §1.2 |
| 6 | 复核 `V6_M0_TEST_PLAN.md` §0 的 **8 条取证规矩**是否接受为本阶段标准 | 流程 | `V6_M0_TEST_PLAN.md` |
| 7 | `HEALTH` 由 `ticks % 10` 改按时间判定；`mac type is incorrect` 的读取时序 | 代码 | 审计报告 §5 |

**需要用户明文授权才能做的**（不在 WorkBuddy 权限内）：回滚写回实测
（`V6_M0_RECOVERY_WRITEBACK_PLAN.md`，段 A 零净变更 / 段 B 全片 32 MiB），授权语句已写在方案 §2。

---

## 6. 我这次交付了什么（改动清单）

相对基点 `1b52240`，**15 笔提交，9 个新文件，4247 行新增**；**未修改任何 `main/`、`managed_components/`、
`sdkconfig`、分区表、`vendor/` 或 `E:/v6/s1` 构建树。**

| 文件 | 行数 | 说明 |
| --- | --- | --- |
| `tools/v6/hw_matrix.py` | 504 | 只读矩阵采集器：解析串口/刷写捕获 + 冻结候选 → JSON + Markdown；带判定不变量 |
| `tools/v6/test_hw_matrix.py` | 361 | **26 项**单元测试，全部合成 fixture，不依赖私密目录 |
| `integration/v6/m0-hw-matrix.json` | 1866 | 矩阵结构化产物（来自 14 份启动捕获 + 1 份刷写捕获） |
| `docs/v6/V6_M0_HW_MATRIX.md` | 133 | 自动生成的验收矩阵 |
| `docs/v6/V6_M0_NETWORK_PROBE.md` | 358 | 网络链路逐轮取证（含隐藏 SSID 根因与反证实验） |
| `docs/v6/V6_M0_TEST_PLAN.md` | 189 | 后续测试计划（8 条规矩 + 每项的刺激/命令/判据/责任人） |
| `docs/v6/V6_M0_RECOVERY_WRITEBACK_PLAN.md` | 118 | 回滚写回方案（**未执行**） |
| `docs/v6/V6_M0_WAKEWORD_INSTRUMENTATION_PROPOSAL.md` | 174 | 唤醒词插桩提案 + 可直接应用的补丁 + 判据表 |
| `docs/project_management/reports/WB-V6-M0-AUDIT_2026-09-22.md` | 544 | 主审计报告（含 §1.1 的 SUPERSEDED 更正） |

`python -m unittest discover -s tools/v6 -t tools/v6` → **40 passed**（你的 12 + 我的 28）。

---

## 7. 复检入口（每条都能自己跑一遍）

```bash
# 全量工具测试
python -m unittest discover -s tools/v6 -t tools/v6 -p "test_*.py"        # 40 passed

# 矩阵再生（14 份捕获 + 1 份刷写；私密日志在 out/v6-device-private/，已 gitignore）
python tools/v6/hw_matrix.py scan --boot <各捕获>... --flash <刷写日志> \
  --candidate integration/v6/m0-candidate-04.json \
  --json-out integration/v6/m0-hw-matrix.json --md-out docs/v6/V6_M0_HW_MATRIX.md

# 崩溃证据（§1.1）
grep -n -E "Calling app_main|abort\(\) was called|I2C|SW_CPU_RESET|BOOT_READY" \
  out/v6-device-private/netprobe-m0-07.txt

# 设备上的唤醒词模型（§1.2）
python -m esptool --chip esp32p4 -p COM7 -b 460800 read_flash 0x10f000 0x48000 dev-sr.bin
#   前 291,042 B 的 SHA256 应为 7c87dd7adb5a7623907b6354d49bea7f9289371262e6a57f15458fb8908c5814

# 主机侧（M1 后端已预验证）
cd backend && <venv>/python -m pytest -q                                  # 79 passed
```

---

## 8. 我不宣称的东西（请勿从本报告外推）

1. **不宣称 M0 通过**：启动已 FAIL，唤醒词未验证，回滚未实测。
2. **不宣称音频质量通过**：回环可闻只证明麦克风与扬声器**双向通路有据**，
   音质/音量/AEC/唤醒率均无量化证据。
3. **不宣称 C5 射频"通过"**：取得 IP 证明**关联**成功；长稳、断线重连、弱信号未测。
4. **不宣称回滚可用**：备份可读可哈希 ≠ 能写回，恢复写回从未执行。
5. **不把"设备能主动连到主机 relay"算作已验证**：M0 诊断固件**没有 relay 客户端**，
   该项必须等 M1 固件；主机与设备同处一个 L2 已单独验证（同 SSID / 同 AP BSSID / 同 /24）。
6. **不宣称长稳**：最长连续观测 110 s，`Flash / PSRAM` 行的"长期压力未测"仍然成立。

---

## 9. 取证口径（建议作为本阶段标准，见 `V6_M0_TEST_PLAN.md` §0）

8 条规矩，每条都有踩过的实例：① 报负结论前先证观测口有效（附 `file:line`）；
② 一条证据只证一件事，缺信号 = `NOT_VERIFIED`；③ 日志字符串 ≠ 事实，去读打印它的源码；
④ 取证窗口必须大于上游超时常量；⑤ 跨捕获按最大值合并，新日志不得抹掉旧证据；
⑥ 关键事实两侧独立复核；⑦ 沙箱内 `curl`/socket/`ping` 都不能判断局域网可达性；
⑧ 给工具传路径写绝对路径、不做 `../` 拼接。

---

## 10. 交接方式

1. **代码与证据**：本报告、全部 9 个文件都在分支 `workbuddy-v6-m0-closure-tools`（15 笔提交，基点 `1b52240`）。
   最新一笔 `6340ef3` 需用户 push（沙箱内无法 push，实为环境限制）。
2. **GitHub**：`https://github.com/revercgy-hub/claw4-learning-habit-ai/tree/workbuddy-v6-m0-closure-tools`
3. **私密原始数据不交接、不提交**：`out/v6-device-private/`（被 `.gitignore` 覆盖）内含
   原厂 32 MiB 全量备份、14 份串口原始日志、NVS 回读（**含 WiFi 口令**）。
   本报告与矩阵只引用其文件名与 SHA256。
4. **建议你的复检顺序**：§1.1（启动循环）→ §1.2 + 插桩提案 → §4.1（隐藏 SSID）→ §4.2（gain，先量再改）。
