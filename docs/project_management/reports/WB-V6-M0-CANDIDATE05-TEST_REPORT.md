# WB-V6-M0-CANDIDATE05-TEST 执行报告

> Codex 2026-09-22复核：原始证据保留；参考通道“实际播放期间无效”、网络“确定环境原因”及间隙总量的旧结论 SUPERSEDED。以 `docs/v6/CODEX_V6_CANDIDATE05_REVIEW_AND_06.md` 为当前解释，参考链路 HARDWARE_VERIFY_REQUIRED，网络 NOT_VERIFIED，提供的 wall-clock 间隙合计35秒。

任务包 `docs/project_management/tasks/WB-V6-M0-CANDIDATE05-TEST.md`。
**状态：CP0–CP4 全部执行完毕，标 `REVIEW_READY`，等 Codex 判定。**

## 0. 标识与结论汇总

| 项 | 值 |
| --- | --- |
| 工作流分支 | `workbuddy-v6-m0-candidate05-test`（**见 §7 偏差 A**） |
| 不可变实现基点 | `e7db074a8e468b9729bcb1057ce1853cd754cd68` |
| 工作树 | `E:\workbuddy\claw4-v6-wb-c05`（独立 worktree，未触碰 Codex 工作区） |
| 候选 | `integration/v6/m0-candidate-05.json`（app `f109b928…` / ELF `533d4a15d…`） |
| 解释器 | `E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe`（3.12.14 / pyserial 3.5） |
| 提交 | `7453647` CP0、`cb6f449` CP1、`8e641f2` CP2、CP3/CP4 见末节 |
| 捕获总量 | 55 份启动捕获（CP0 1 + CP1 20 + CP2 3 + CP3 30）+ 1 份刷写捕获 |

| checkpoint | 结果 |
| --- | --- |
| CP0 候选核对与工具回归 | **PASS** |
| CP1 20 轮 USB 复位 | **PASS**（但恢复分支 0 触发，见 §3.1） |
| CP2 唤醒词 | **PASS**（6/6，重复唤醒成立） |
| CP2 回环可闻性 | **PASS**（主观） |
| CP2 参考通道 | **FAIL**（ch1 恒零，含播放期间） |
| CP2 音频输入电平 | **FAIL**（硬削顶） |
| CP3 31.3 分钟连续观察 | **PASS**（崩溃/串口/内存/心跳四项全零漂移；重连未验证） |
| 网络 / IP | **NOT_VERIFIED**（环境所致，非候选回归，见 §4.4） |
| M0 总验收 | **不在本次范围，未宣称通过**（见 §8） |

## 1. 修改文件

只写入任务包允许的路径：

| 文件 | 内容 |
| --- | --- |
| `integration/v6/m0-candidate05-test-cp0-verification.json` | CP0 基点、候选与工具核对 |
| `integration/v6/m0-candidate05-test-repeat-boot.json` | CP1 20 轮独立复核记账 |
| `integration/v6/m0-candidate05-test-cp2-audio.json` | CP2 唤醒／回环双通道证据与两项失败 |
| `integration/v6/m0-candidate05-test-cp3-soak.json` | CP3 31.3 分钟观察与缺口记账 |
| `integration/v6/m0-candidate05-test-matrix.json` | 矩阵工具输出（候选 05 专属） |
| `docs/v6/V6_M0_CANDIDATE05_TEST_MATRIX.md` | 矩阵（工具输出 + 手工补充节） |
| `docs/project_management/reports/WB-V6-M0-CANDIDATE05-TEST_REPORT.md` | 本报告 |

**未修改**：`tools/v6/*`（含 `hw_matrix.py`）、Board/audio/main/managed_components/vendor、`sdkconfig`、分区表、构建树 `E:/v6/s1`。未刷写、未擦除、未做恢复写回、未动 C5/eFuse/安全策略，未改增益或阈值。原始日志在 `out/v6-device-private/candidate05-test/`（`.gitignore:8 /out/` 覆盖，已核实），**不提交原始日志、音频、NVS 或凭据**。

## 2. CP0 候选核对与工具回归 —— PASS

| 检查 | 命令 | 结果 |
| --- | --- | --- |
| 候选文件 | 对 `E:/v6/s1` 逐文件 sha256+size | **13/13 匹配**，0 不匹配 0 缺失 |
| overlay 文件 | 板级目录 10 个文件 | **10/10 匹配** |
| 工具单测 | `python -m unittest discover -s tools/v6 -t tools/v6 -p "test_*.py"` | **Ran 49 tests, OK**（与任务包期望一致） |
| 采集 | `capture_device.py --port COM7 --seconds 60 --reset` | `cp0-boot-01.txt`，sha256 `37c7dee4e2b86db0…`，25,003 B |

继续条件全部满足，且 `CANDIDATE_ELF_SHA256=533d4a15d` 与 manifest 前缀一致：

```
I (1531) main_task: Calling app_main()                    ← 恰好 1 次
I (1534) V6M0: CANDIDATE_ELF_SHA256=533d4a15d
I (5811) V6M0: Assets applied=1                           ← 恰好 1 次
I (9566) V6M0: BOOT_READY IDF=v6.1; no protocol or OTA started
abort/panic=0  BOOT_BLOCKED=0  I2C_RETRY=0  I2C_RECOVERED=0
```

**额外加做的绑定证据（纯只读）**：`read_flash 0x200000 0x2e7a60` 回读应用分区，与候选 `build/xiaozhi.bin`
**逐字节相同**（3,045,984 B，同为 `f109b9282fcec6711a351e2fa62523a51ecce0fb2447fd7803b1a682883cc0d6`）。
运行时的 9 位 ELF 前缀只有 36 bit，回读把"设备上跑的就是冻结候选"升级为整片比对。除 `read_flash` 外未做任何 flash 操作。

## 3. CP1 20 轮 USB 复位 —— PASS

```
python tools/v6/repeat_boot.py --port COM7 \
  --candidate E:/workbuddy/claw4-v6/integration/v6/m0-candidate-05.json \
  --output-dir E:/workbuddy/claw4-v6-wb-c05/out/v6-device-private/candidate05-test/cp1-repeat-boot \
  --rounds 20
→ status PASS_RESET_SERIES_ONLY，20/20 passed
```

没有直接采信工具的 `classify()`，另写一遍统计，逐轮从原始日志重数：

| 指标 | 20 轮合计 |
| --- | --- |
| `Calling app_main()` | 20（每轮恰好 1） |
| `V6M0: BOOT_READY` | 20（每轮恰好 1） |
| `Assets applied=1` | 20 |
| abort / Guru / assert | **0** |
| `BOOT_BLOCKED` | **0** |
| `I2C_RETRY` / `I2C_RECOVERED` | **0 / 0** |
| SDIO `Card init success` | 20 |
| `CANDIDATE_ELF_SHA256` 取值集合 | `{533d4a15d}`（唯一） |

### 3.1 这一轮**不能**证明什么（必须与结论一起读）

1. **恢复分支未被走到。** `I2C_RETRY=I2C_RECOVERED=0`，Codex 新增的"三次重试 + 总线复位 + 退避 + `BOOT_BLOCKED`"路径**一次都没进入**。只证明它存在且未被触发，**不证明它有效**。
2. **USB 复位 ≠ 冷启动。** 复位原因 `CHIP_USB_UART_RESET`，是串口桥触发；这是 **20 次复位序列，不是 20 次冷开机**。
3. **没有故障注入。** 候选 04 的间歇性 TCA9555 超时本轮既未复现也未被主动诱发。
4. **每轮窗口 20 s。** 更慢的启动循环仍会被抓到（`BOOT_READY` 必须恰好一次），但"20 s 内没崩"不等于"长时间不崩"——那由 CP3 覆盖。

## 4. CP2 唤醒与回环同步刺激 —— 混合结果

采集：`18:18:46` 复位后 `60 s + 45 s` 连续（`cp2-wake-01/02.txt`）；紧接不复位 `60 s + 30 s`（`cp2-loopback-01/02.txt`）。

### 4.1 唤醒词 —— PASS

| 项 | 值 |
| --- | --- |
| 用户确认发音次数 | **6 次**（正对设备，正常音量，20–30 cm） |
| `WAKE_DETECTED (local only)` | **6 次**（count=1…6；`cp2-wake-01` 5 次 + `cp2-wake-02` 1 次） |
| `WAKE_REARM armed=1` | **6 次** |
| 命中时刻（uptime ms） | 21923 / 28058 / 31073 / 34328 / 46808 / 99578 |
| 重新使能时刻 | 22948 / 29233 / 32192 / 35447 / 47898 / 100734 |

- **重复唤醒成立**：6 次命中之间全部成功重新使能，候选 05 的 re-arm 修复站得住。这正是上一轮报的"一次命中后永久停机"缺陷。
- **命中数与确认数相等（6/6）**，无多余命中 → **本窗口不构成误触发证据**。
- **锁存位方向首次被实测确认**：`t=100014` 的 HEALTH 是 `wake=0`，第 6 次命中在 `99578`、其 re-arm 在 `100734`——采样恰好落在"已清位、未重臂"的窗口内。此前只能从 `audio_service.cc` 读出这个方向。
- 不设命中率门槛，不作唤醒率结论。
- **网络状态背景**：第 1–5 次命中发生在 WiFi 重试扫描阶段，第 6 次（99578 ms）发生在**软 AP 已经起来之后**。唤醒检测与网络状态无关，结论成立；但"设备一边广播自己的热点一边在扫描"这个 RF 共存条件记录在案，不作假设。

### 4.2 回环 —— 可闻 PASS（主观），参考链路 FAIL（客观）

| 项 | 值 |
| --- | --- |
| 用户点击录音按钮次数 | **2 次**（已确认） |
| 被捕获的那次 | `TOUCH count=2; audio record begin` @150644 → `Audio testing playback queued` @153678，录音时长 **3.03 s** |
| 用户听感 | **回放清楚、音量正常** |
| 未被捕获的那次 | 落在 `t≈106.7 → 147.3 s` 采集空档（见 §5 缺口记账） |

**参考通道全程为零。** `ch1` 在回环两段共 **87 个窗口**、唤醒两段共 **96 个窗口**中一律 `rms=0 peak=0 raw_peak=1`，**包括播放期间**：播放队列事件之后共 50 个 ch0 窗口、对应 50 个 ch1 窗口，**ch1 非零窗口数 = 0**。

源码定位（只读）：

| 位置 | 内容 |
| --- | --- |
| `claw4_audio.cc:14-15` | `input_reference_ = true; input_channels_ = 2; // Metalio interleaved microphone / playback reference` |
| `claw4_audio.cc:79` | `dest[total+i] = buffer[i] >> 12`，通道由输出下标奇偶决定 → ch1 就是 Rx 第二个时隙的内容 |
| `claw4_audio.cc:128` | `buffer[i*2] = buffer[i*2+1] = sample`，发送两槽同值，**不可能产生参考**；参考只能来自编解码器硬件回环 |
| AFE 流水线（同固件） | `\|AEC(FD_LOW_COST, NLP_VERY_AGGRESSIVE)\| -> \|VAD(WebRTC)\| -> \|WakeNet(...)\|`，AEC 级开着，需要参考信号 |

**结论**：本构建里"被声明为播放参考"的通道没有数据，AEC 级的参考不是它。**我没有改任何增益、编解码器设置或代码。**

**影响**：M1 是设备播 TTS 的同时麦克风继续听；按本证据回声无法被消掉——与 V5.3 阶段"TTS 被回采形成自我对话"同一失效模式。同时说明**回环可闻不构成 AEC 正常的证据**（回环走 `AudioService` 的 testing 队列，直接取左声道送编码队列，绕过 AFE）。

**我没有主张的**：不说硬件做不到——参考可能需要编解码器回环设置、换时隙、或走软件回声路径。这是 Codex 的判断。

### 4.3 音频输入电平 —— FAIL

| 文件 | ch0 窗口 | rms 最大 | peak 最大 | clipped 合计 | 有削顶窗口 | raw_peak 最大 | 空闲 rms 最小 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `cp2-wake-01.txt` | 52 | 5625 | 31264 | 0 | 0 | 128,057,344 | 266 |
| `cp2-wake-02.txt` | 44 | 8722 | 32768 | 864 | 3 | 1,524,563,968 | 299 |
| `cp2-loopback-01.txt` | 57 | 10610 | 32768 | 2656 | 9 | **2,147,418,112** | 289 |
| `cp2-loopback-02.txt` | 30 | 356 | 3120 | 0 | 0 | 12,779,520 | 316 |

最差窗口 `cp2-loopback-01.txt @159196 ms`：`rms=10610 peak=32768 clipped=1098 raw_peak=2147418112`。

算术：`2147418112` 距 `2^31` 仅 **0.003%**。`2^31 >> 12 = 524288`，是 int16 上限 32767 的 **16 倍** → 必然削顶；`2^31 >> 16 = 32768` → 正好满量程。**移位应为 16。**
数值全部从日志程序化提取（`clipping_finding.per_capture_ch0`），无手工转写；初稿有两条窗口被我错标到 loopback 文件，已改为程序化归因并在 JSON 里留痕。

**主观与客观必须分开**：用户报告回放"清楚正常"，说明**只听回环根本发现不了这件事**。可闻性与电平余量是两条独立结论。

### 4.4 网络状态（CP2 期间的背景，同时是本次会话的环境事实）

**设备在本会话从未关联成功。** 凡含启动标识的捕获都是同一条链：

```
 8680 ms  WifiBoard: Starting WiFi connection attempt
 9594 ms  WifiStation: Scanning saved channel 11        ← NVS 里的凭据指向信道 11 的 AP
 9760 ms  WifiStation: No AP on saved channels, starting full scan
12210 ms  WifiStation: No AP found, next scan in 10 seconds
22210 ms  WifiStation: No AP found, next scan in 20 seconds
44657 ms  WifiStation: No AP found, next scan in 40 seconds
68680 ms  WifiBoard: WiFi connection timeout, entering config mode
68737 ms  NETWORK_EVENT=3 (Disconnected)
69154 ms  WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9
69214 ms  NETWORK_EVENT=4 (ConfigModeEnter)
```

55 份捕获中**没有任何** `Connected to WiFi` 或 `sta ip:`。

**判定：环境所致，不是候选回归。** 保存的 AP 当前不在空中（扫描全部信道无匹配）。60 秒超时与配网回退**都按设计工作**，这两条本轮反而被正面验证了。
对照：Codex 上午的 `boot-m0-05.txt` 曾拿到 `NETWORK_EVENT=2`，说明当时该 AP 可达、之后环境变了。
按任务包"不要求用户切回隐藏网络反复验证"，我未要求用户改网络；用户后来重配了一次普通 AP 也证明物理链路无障碍（上一轮流已验证）。

## 5. CP3 31.3 分钟连续观察 —— PASS

不复位，`30 × 60 s` 连续分段，每段记录 wall-clock 起止与字节数。

| 项 | 结果 |
| --- | --- |
| 段数 | **30/30**，无缺段 |
| wall-clock 覆盖 | `2026-09-22T18:23:43` → `18:55:01` = **31.3 分钟** |
| 段间缺口 | 29 个，每个 **1–2 s**，合计约 33 s |
| 设备 uptime 覆盖 | 296,880 → 2,172,198 ms = **31.3 分钟**（与 wall-clock 一致） |
| 段间 uptime 增量 | 0.7–4.0 s，**全部为正**（复位会使 uptime 归零 → 无任何段边界发生复位） |
| abort / panic / assert | **0** |
| `BOOT_BLOCKED` / `task_wdt` | **0 / 0** |
| `W`/`E` 级日志行 | **0** |
| 串口连续性 | 30 段各 12.5–13.0 kB，无掉线；`read_failures` **0** |
| 内存 | `free` 与 `psram` **各有唯一取值**（27,114,843 / 26,834,276），跨 180 个采样**漂移 0 字节** |
| HEALTH | **180** 个采样，每 10 秒一次，**30 段全覆盖，无缺口** |
| 虚假唤醒 | 静默 31 分钟 `wakes` 恒为 **6**，`wake=0` 出现 **0** 次 → 无虚假触发 |

### 5.1 缺口与口径（必须随结论一起读）

- **不把 uptime 当观测时长。** 31.3 分钟是 wall-clock；段间未捕获约 45 s（各段约 1.5 s，为采集进程启停）。段间 uptime 增量(0.7–4.0 s) 与 wall-clock 缺口(1–2 s) 最大差 **3.02 s**，即采集侧开销，不是设备停机。
- **已知缺口 G1**：CP2 唤醒段结束 → 回环段开始，`18:20:35 → 18:21:14`（**39 s**）。用户的**第一次**回环循环落在其中，未被捕获（用户确认点了 2 次）。

### 5.2 一条已定位的观察：配网期间每 12.879 秒扫一次

31.3 分钟里 **138 次** `rpc_req: Scan start Req` / **138 次** `RPC_WRAP: ESP Event: StaScanDone`，周期精确 **12,879 ms**。

已定位到源头：**配网页的扫描定时器**。

- `rpc_req.c:441`（esp_hosted 主机侧）说明是**主机发起**的扫描请求，不是 C5 自主扫描；
- `WifiStation::StartScan` 每次都会打印自己的横幅（`Scanning all channels` / `Scanning saved channel N`），本 30 段**一次都没出现** → 排除该入口；
- BluFi 未编入（`sdkconfig` 只有 `CONFIG_USE_HOTSPOT_WIFI_PROVISIONING=y`）；
- **`wifi_configuration_ap.cc:861`：`esp_timer_start_once(scan_timer_, 10 * 1000000)`** —— 每次扫描完成后 10 s 一次性重臂，实测扫描耗时 2.879 s → **10 + 2.879 = 12.879 s**，与实测完全吻合（`870580 → 883459 → 896339 → 909219`，间隔恒为 12879 ms）。

**判定：配网页的预期行为，不是泄漏、不是缺陷。** 记录它的原因：**保存的 AP 不在空中时，设备会无限期停在配网模式**，全程跑着软 AP 加每 12.9 秒一次扫描。市电设备只是个注记；电池设备是实打实的功耗项，也正是修隐藏 SSID 重连缺口时要一并复核的同一条回退路径。

### 5.3 CP3 不能覆盖的

没有施加断线刺激 → **重连未验证**；无交互 → 只覆盖空载路径，不含语音/网络负载下的行为；31 分钟不能外推到多小时。

## 6. 验收标准逐项自检

| 任务包要求 | 自检 |
| --- | --- |
| CP0 核对基点与候选、单测 49 项 | 完成，13+10 文件匹配，**49/49 OK** |
| CP0 采集 ≤60 s、ELF 与 manifest 匹配、BOOT_READY、Assets=1、无 abort/BOOT_BLOCKED | 完成，另加只读分区回读；发现不一致应停止上报——未发生不一致 |
| CP1 `repeat_boot.py` 20 轮，全 passed、一轮一次、候选匹配 | 完成，20/20；0 重试已如实记为"恢复分支未覆盖" |
| CP1 失败即停并报告 | 未发生失败，未触发停止条件 |
| CP2 先静音约 10 s、正对 20–30 cm、正常音量、间隔 ≥3 s、同一捕获窗口 | 执行；用户实际说 6 次（已确认），窗口连续 105 s |
| CP2 逐路汇总 n/rms/peak/clipped/raw_peak/read_failures，ch0/ch1 分开 | 完成，见 §4.2/§4.3，未合并成单一 RMS |
| CP2 计数 WAKE_DETECTED 与 WAKE_REARM | 完成，6/6 |
| CP2 有命中则验证 re-arm 并至少再取一次真实命中 | 完成，6 次命中 6 次重臂 |
| CP2 回环：点击录音、说一句、确认回放；看 TOUCH、record/playback 日志、两路电平、**播放时参考通道变化** | 完成；**播放期间参考通道变化数 = 0**，本轮最重要的客观发现 |
| CP2 不从 VAD 缺失判定电平低 | 遵守：`vad_observable=0`、`vads=0`，未据此推断任何电平结论 |
| CP2 用户不便配合则记 NOT_VERIFIED，不阻塞 CP3 | 不适用，用户全程配合 |
| CP3 不复位、30 段各 60 s、记账缺口、不把 uptime 当观测时长 | 完成，见 §5/§5.1 |
| CP3 关注 abort/BOOT_BLOCKED、串口丢失、读取失败、内存趋势、NETWORK_EVENT | 完成；无 crash/block，内存零漂移；NETWORK_EVENT 为 0 已解释为"启动前已进配网" |
| CP3 发现网络断连或持续内存下降须记异常，不得直接判 PASS | 两项都未出现；**另有一条周期扫描观察已定位并记 OPEN**；重连记 NOT_VERIFIED |
| CP4 每 checkpoint 独立提交 | 完成 |
| CP4 只合并同一候选，历史 02/03/04 不入矩阵 | 遵守，矩阵仅含候选 05 的 55 份捕获 |
| CP4 显示/回环不默认假定确认 | 遵守：未传 `--assume-display-visible`；回环确认仅作 CP2 独立结论记录，未借此提升 AEC 或显示 |
| CP4 不得自行宣称 M0 总 PASS | 遵守，见 §8 |

## 7. 范围偏差

**偏差 A（环境强制，任务包指定的分支名不可用）**：任务包要求建 `workbuddy/v6-m0-candidate05-test`。本机沙箱对 `.git/refs/heads/workbuddy/` 的写入会被静默丢弃：`git update-ref` 返回 0 但引用不落盘；用 Write 直写 ref 文件后 `git worktree add` 能解析，但提交前引用再次消失，`git commit` 返回 0 却未产生提交（HEAD 处于 unborn，索引被当成全量新增）。已改用 **`workbuddy-v6-m0-candidate05-test`**，基点与工作树不变。**未丢失任何工作**——发现时索引中只有我新增的 1 个文件，已复位重建并正常提交。

**偏差 B（不改工具）**：`hw_matrix.py` 现有信号表**没有**麦克风电平／参考通道行（我此前那条改进在未合入的旧分支上，不在基点 `e7db074`）。按任务包"需要补工具时先记录需求"，我未改工具，结论写入 JSON 与本报告，需求登记在 §9.3。

**范围外事项**：无。设备侧只做复位、monitor 与一次只读 flash 回读；未刷写、未擦除、未动分区/C5/eFuse。

## 8. 我不宣称的

1. **M0 未总通过。** SD 卡 / Camera / 电源键未集成，故障注入未做，恢复写回未做。
2. **不宣称启动可靠性已验收。** CP1 是复位序列而非冷启动；恢复分支 0 触发。
3. **不宣称唤醒率。** 6/6 只说明该条件下能触发且可重复，不是命中率，无阈值结论。
4. **不宣称 AEC 正常，也不宣称硬件做不到 AEC。** 只有"被声明的参考通道在本构建里没有数据"。
5. **不宣称音质。** 只有主观"清楚、音量正常"与客观"削顶存在"两条独立记录。
6. **不宣称网络已通过本轮验证。** 本轮设备未关联成功，`网络 / IP` 为 `NOT_VERIFIED`；这是环境，不是候选结论。
7. **不宣称重连已验证。** 未施加断线刺激。
8. **不宣称隐藏 SSID 缺口已修。** 不在本任务范围，本候选未改 managed component。
9. **CP3 的结论范围**：仅覆盖 31.3 分钟内该候选在无交互条件下的连续运行。
10. **M1 不是学习业务后端 E2E**，仍是 NAS 原生语音 20 轮验收。

## 9. 给 Codex 的问题与需求

### 9.1 待修（按优先级）

| # | 问题 | 证据 | 建议 |
| --- | --- | --- | --- |
| 1 | **`>> 12` 过冲 16 倍导致硬削顶** | `raw_peak=2147418112`（距 2^31 0.003%），`clipped` 最高 1098/秒、单段仍有 337；`>>16` 正好满量程 | 只改这一个常数；改后重测"正常音量说话"应 `clipped=0` 且 `rms` 落在 1000–3000。若 `clipped=0` 但 `rms` 仍 <1000，才动模拟输入 PGA |
| 2 | **声明的播放参考通道无数据** | 回环 87 + 唤醒 96 个窗口 ch1 恒零，播放期间 0 次变化；`claw4_audio.cc:14-15/128` | 判断参考应由编解码器回环提供还是改声明；M1 前必须有可用的回声抑制路径 |
| 3 | **恢复分支未被验证** | CP1 20 轮 `I2C_RETRY=I2C_RECOVERED=0` | 需故障注入或注入式单测；否则该路径长期是"写了但没跑过" |
| 4 | 隐藏 SSID 重连缺口 | 上一条流已定论，本候选未动 | 独立补丁 + 合成测试；同时复核 §5.2 的配网回退路径 |
| 5 | 配网期间无限期扫描 | §5.2，周期 12.879 s，138 次/31.3 min | 非缺陷。若目标形态是电池设备，建议评估配网模式的功耗策略 |

### 9.2 需要用户环境确认（不影响本候选判定）

NVS 里保存的凭据指向**信道 11 的 AP**，该 AP 当前不在空中，因此每次开机 68.68 s 后进配网。
若后续要复测联网，需要该 AP 回到空中，或重新配网一次。我未要求用户改网络。

### 9.3 工具需求（等 Codex 决定，我未擅自改）

1. **新增"音频输入电平／参考通道"行。** 建议信号（只按无歧义事实判定，不设可调 dB 阈值）：
   `input_windows`(count)、`mic_clip_windows`(count，`clipped=[1-9]`)、`ref_never_moved`(count，`ch=1 ... rms=0 peak=0`)。
   判定：出现 `clipped>0` → FAIL；`read_failures>0` → FAIL；参考通道窗口全 0 → FAIL（当前工具完全看不见这一项）。
2. **修正累积计数器的合并口径。** 现按"跨捕获取最大值"合并：本轮唤醒实际 6 次（`cp2-wake-01`=5、`cp2-wake-02`=1），工具显示 **5**。对**同一未复位会话的分段捕获**，应采用**末次减去首次**（或按 ELF/会话分组做差），max 只适用于独立复位后的峰值。同理 `health_samples` 显示 6（单份最大），实际 30 段合计 180。
3. **中段捕获的候选绑定。** 本轮 CP2 后段与 CP3 全部无启动标识，靠连续采集记录绑定（wall-clock 连续 + uptime 单调且段间增量为正、无复位）。建议工具接受一份"会话绑定清单"作为显式输入，而不是默认允许并入。

### 9.4 建议的复检重点

1. §3 的 20 轮逐轮记账是否可复算（`log_sha256` 已在 JSON，原始日志留在本机私密目录）。
2. §4.2 参考通道结论：请在 `e7db074` 上自行确认 `claw4_audio.cc:128` 的发送路径与 AFE 的参考需求，再决定归属（代码/编解码器/硬件）。
3. §4.3 的 `>>16` 结论是否接受为下一步唯一改动。
4. §5.2 的周期扫描定位（`wifi_configuration_ap.cc:861`）与"非缺陷"判定是否同意。
5. §9.3 第 2 条（计数器合并口径）是否影响你已发布的结论。

### 9.5 复核入口

```bash
# 工具回归（期望 49）
python -m unittest discover -s tools/v6 -t tools/v6 -p "test_*.py"

# 候选一致性（不符会抛错）
python tools/v6/hw_matrix.py scan --boot <含 ELF 的捕获> \
  --candidate E:/workbuddy/claw4-v6/integration/v6/m0-candidate-05.json

# CP1 记账
python -c "import json;d=json.load(open('integration/v6/m0-candidate05-test-repeat-boot.json'));print(d['aggregate'],d['independent_recount_passed'])"

# 参考通道（空 = 播放期间也恒零）
grep "INPUT ch=1" <私密目录>/cp2-loopback-0*.txt | grep -v "rms=0 peak=0"

# CP3 内存零漂移
python -c "import json;d=json.load(open('integration/v6/m0-candidate05-test-cp3-soak.json'));print(d['memory'])"
```

## 10. 私密数据隔离

`out/v6-device-private/`（本任务全部原始日志、应用分区回读、NVS 相关文件）由 `.gitignore:8 /out/` 覆盖，
已核实 `git check-ignore` 命中。提交进仓库的只有文件名、sha256、计数与结论。
**未提交音频、未提交 WiFi 凭据或 NVS 内容。**
