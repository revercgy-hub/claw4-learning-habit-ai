# V6 M0 候选 08 独立复核验收矩阵

> 流：`WB-V6-M0-CANDIDATE08-REVIEW`（任务书 `6ed2a39` 修订版）
> 分支：`workbuddy-v6-m0-candidate08-review`（连字符名；任务书要的带斜杠名在本机沙箱无法持久建立，见 §4.5 DEV-08-2）
> 冻结实现基点：`1907730926d383072dc267201c6f359c4a001231`
> 候选：`claw4-learning-v6-m0.8`，app SHA256 `1ef710aec525df3f91142d9ca6188b26e6881e448e72007804515a7f6ae6e7c2`，3,051,504 B
> ELF SHA256 `50fb62c30f8e82797f40ee6e3ab085cebc9281986883db06823d61da761708ca`

**判定口径**

1. 缺证据一律 `NOT_VERIFIED`，绝不由相邻项推断。**本包内不允许任何跨候选借用**：候选 05/06/07 的结论只在明确标注处作为*基线引用*出现，不作候选 08 结果。
2. 有反面证据记 `FAIL`；没有证据记 `NOT_VERIFIED`；二者不可互换。
3. 证据分四类独立列出（CODE / HOST / BUILD / DEVICE），**一类证据不能顶替另一类**。
4. 主动声明不成立的说法集中在报告 §8，避免从本文档外推。
5. 本矩阵不构成 M0 总通过。

---

## 1. CODE 证据（只读静态复核，CP0）

| 项目 | 证据 | 状态 | 说明 |
| --- | --- | --- | --- |
| 配置失败不得沿用旧参数连接 | `wifi_station.cc:381-385` | **PASS** | `esp_wifi_set_config` 失败后打印 `CONNECT_CONFIG_FAILED` 并 `continue`，不调用 `esp_wifi_connect()` |
| 同步连接失败必须有界退避且不 abort | `wifi_station.cc:387-392` | **PASS** | 失败打印 `CONNECT_START_FAILED` 后推进；有限队列由 `while` 排空，无递归、无 `ESP_ERROR_CHECK`，随后按当前退避重臂扫描定时器 |
| 失败的扫描完成事件不得误触发隐藏 SSID 回退 | `wifi_station.cc:239-250`、`:479` | **PASS** | `HandleScanDone(false)` 清队列、清 AP 列表、记 `SCAN_COMPLETION_FAILED` 并退避，**不进入** `HandleScanResult`；另有 `scan != nullptr && scan->status == 0` 的门，事件载荷缺失时同样 fail-closed |
| 回退触发是否局限"全信道无匹配" | `wifi_station.cc:299-320` | **PASS** | 回退位于 `last_scan_used_saved_channels_` 升级分支之后，只在"成功扫描且已扫全信道"时执行 |
| 可见 AP 优先 / pin 限制 / 凭据边界 / 三项上限 | `saved_network_policy.h:13-22`、`wifi_station.cc:275-296`、`:309-315` | **PASS** | 可见匹配先填 `connect_queue_`（RSSI 排序）并被优先消费；pin 时策略返回空；上限 3，跳过空/超 32/超 64/含 NUL 的凭据**而非截断**，按 SSID 去重；回退项标 direct、channel 0、无 BSSID |
| direct 状态在首次失败 / 获得 IP / 重启站点后的变化 | `wifi_station.cc:386`、`:492`、`:512-536`、`:116-126` | **PASS** | direct 项置 `reconnect_count_ = MAX_RECONNECT_COUNT`，跳过上游对同一回退项的 5 次重试；获得 IP 清队列、归零计数、恢复 `use_saved_channels_scan_` 与扫描间隔；`Start()` 每次复位同一状态 |
| 异步断线与配网停止边界 | `wifi_station.cc:59-98`、`:480-507` | **PASS（含 1 项观察）** | `Stop` 先注销两个 handler 实例、再停删扫描定时器、最后才置 STOPPED 位，故停止后无回调/定时器可触发；断线路径有界。**观察**：`:493` 断线重试分支调用 `esp_wifi_connect()` **未检查返回值**，同步失败时不重臂定时器、不产生后续事件 |
| overlay 完整性与 CMake 实际使用路径 | `integration/v6/network/overlay.json`、`build/compile_commands.json`、构建日志 | **PASS** | 24 个记录文件中 22 个与固定上游提交逐字节相同，仅 2 个声明为 `replacements` 的目标不同，无非预期差异；缺失的 `.component_hash`/`CHECKSUMS.json` 正是组件 README 说明"因描述未修改包而省略"；多出的 `include/saved_network_policy.h` 为项目自有策略。`tools/v6/network_overlay.py` 全路径 fail-closed（依赖清单不符、锚点计数 ≠ 1、本地覆盖缺失/陈旧/被改均 raise）。构建日志 `Using component placed at E:\v6\s1\components\78__esp-wifi-connect for dependency 78/esp-wifi-connect` 与 `compile_commands.json` 共同证明本机组件被真实选用 |
| 音频 Playback 消费者 / 本地丢包 / 恢复 wake | `m0_diagnostics.cc:82-110`、`:119-122`、`:135` | **PASS（含 1 项观察）** | 录音时先 `EnableVoiceProcessing(true)` 再 `EnableAudioTesting(true)`（注释说明前者会重置解码队列，故须先建立活跃消费者才能在本地播放期间接收）；播放有界（`IsPlaybackIdle()` 或 10 s 上限），且 `LOCAL_REFERENCE_PROBE_END drained=<bool>` 使未排空**可观测**；未排空则 `ResetDecoder()` 并恢复唤醒；AFE 输出本地丢弃上限 8/轮，`vTaskDelay(100ms)` 给出约 80 包/s 的排空能力，高于 60 ms 帧约 17 包/s。**观察**：`:120-126` HEALTH 末字段打印名 `vad_observable=`，实参为 `test == LocalTest::Playback`，即本地播放标志 |

**CODE 类缺陷登记**

| ID | 级别 | 位置 | 复现 | 影响 |
| --- | --- | --- | --- | --- |
| OBS-08-1 | P2 | `wifi_station.cc:492-497` | 读码；硬件上需驱动态故障才能触发 | 同步失败时站点可静默直到板级 60 s 期限。有界，但与同一增量新增的 `StartConnect` 路径处理不对称。为上游既有行为 |
| OBS-08-2 | P2 | `m0_diagnostics.cc:120-126` | 对照格式串与实参表；播放期间该字段读 1 | 证据解读陷阱，非功能缺陷。本项目已多次依据该诊断做判定，故必须登记 |

**无 P0/P1。新日志不含凭据（仅计数与错误码），串口无他人占用，冻结产物与归档一致。**

---

## 2. HOST 证据（主机侧可重复执行的证据，CP0）

| 项目 | 命令 | 状态 | 结果 |
| --- | --- | --- | --- |
| Python 工具单测 | `python -m unittest discover -s tools/v6 -t tools/v6 -p "test_*.py"` | **PASS** | `Ran 62 tests ... OK`（本包**实际重跑**，非引用） |
| C++ 策略测试 | `g++ -std=c++17 -Wall -Wextra -Werror tools/v6/test_saved_network_policy.cc -o out/v6-network-policy-test.exe && ./out/v6-network-policy-test.exe` | **PASS** | 在 `-Werror` 下编译 exit=0；运行 exit=0（静默即成功） |
| 16 生产方法 Host 场景 | `python tools/v6/test_network_station.py --source E:/v6/s1 --cxx E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe` | **PASS** | `PASS: 16 production-method host scenarios; no hardware claims.` 覆盖空队列、隐藏/可见配置失败、可见连接启动失败、首组失败后次组成功、失败扫描完成事件、全扫描启动失败/成功、正常扫描完成事件等 |

**HOST 类的边界（工具自身声明）**：Host shim 不模拟异步事件先后、`Stop` 竞争、RF、C5 或 AP 实际行为。故 HOST 通过**不等于**真机通过。

---

## 3. BUILD 证据（产物与冻结一致性，CP0/CP1）

| 项目 | 证据 | 状态 | 说明 |
| --- | --- | --- | --- |
| manifest 固件文件 | 13 个条目逐项比对 SHA256 与字节数 | **PASS** | 全部匹配 `m0-candidate-08.json` |
| overlay 源文件 | 11 个条目逐项比对 | **PASS** | 全部匹配 |
| 32 MiB 原镜像备份 | `pre-v6-full-flash.bin` | **PASS** | 33,554,432 B，SHA256 `b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413`，与任务书一致 |
| 只读布局检查 | `tools/v6/flash_plan.py --backup <backup> --build E:/v6/s1/build --output <private>` | **PASS（仅布局）** | 列出全部 5 个镜像的落位；**这是布局检查，不是本包的刷写命令**。本包只写 application |
| 新增字面量确在镜像内 | `grep -a -o <lit> E:/v6/s1/build/xiaozhi.bin \| wc -l` | **PASS** | `SAVED_DIRECT_FALLBACK`、`SCAN_COMPLETION_FAILED`、`CONNECT_CONFIG_FAILED`、`CONNECT_START_FAILED`、`LOCAL_REFERENCE_PROBE_BEGIN`、`LOCAL_REFERENCE_PROBE_END`、`WAKE_REARM` 各 **1** 次，证明补丁进了镜像而非只停在源码树。CP0 记录的是其中 4 项子集，此处为完整集 |
| 归档副本一致性 | `build/xiaozhi.bin` vs `out/v6-candidate08-frozen/build/xiaozhi.bin` | **PASS** | 同为 `1ef710ae…` |

---

## 4. DEVICE 证据（真机，CP1/CP2/CP3）

### 4.1 刷写与启动（CP1）

| 项目 | 证据 | 状态 | 说明 |
| --- | --- | --- | --- |
| 设备身份 | `esptool flash-id` | **PASS** | ESP32-P4 revision v1.3，32 MB，MAC `80:f1:b2:d2:ed:14` |
| app-only 写入 0x200000 | 任务书指定命令 | **PASS** | exit 0，`Hash of data verified.`。未刷其它镜像、未擦 NVS、未动分区/C5/eFuse/安全策略、未做恢复写回 |
| 二进制绑定 | 只读回读 `0x200000`，长度由 manifest 计算 | **PASS** | 设备镜像与 `build/xiaozhi.bin` 及 manifest **逐字节相同** `1ef710ae…`。强于运行时 9 位 ELF 前缀 |
| 启动继续条件 | 复位后 60 s 捕获 | **PASS** | `app_main=1`、`BOOT_READY=1`、`Assets applied=1`、`abort/panic=0`、`BOOT_BLOCKED=0`；ELF 前缀 `50fb62c30` 与 manifest 匹配 |

### 4.2 网络 —— 候选 08 新增分支（CP2）

一次复位支撑的 103 s 单次会话，**无人工刺激**（因为所需条件本已成立：既有凭据不在任何扫描结果中）。

| 项目 | 证据 | 状态 | 说明 |
| --- | --- | --- | --- |
| 隐藏 SSID 重启后关联并取得 IP | 设备侧时间线 + 主机侧独立勘测 | **PASS** | 08 要修的就是这条。设备在扫描未列出该 SSID 的情况下经 direct 路径关联并取得 IP |
| 回退仅在全信道无匹配后触发 | `No AP on saved channels` → 全信道 → `SAVED_DIRECT_FALLBACK` | **PASS** | 先扫保存信道，再升级全信道，之后才产生候选 |
| 每候选仅一次连接调用 | 每个候选恰好一条 `WiFi connecting to <ssid>` | **PASS** | 无对同一回退项的 5 次重复，失败 reason 201 后即推进 |
| 有界退避与重试 | `No more AP to connect, next scan in 10 seconds` → 10 s 后重扫 | **PASS** | 队列排空后有界退避，且重试成功 |
| 无驱动层失败标志 | `CONNECT_CONFIG_FAILED` / `CONNECT_START_FAILED` / `SCAN_COMPLETION_FAILED` 计数全 0 | **PASS** | 本次未触及这三条路径 |
| 关联后稳定性 | 36.3 s 至捕获结束 | **PASS** | 无断连、无配网回落、无连接超时，`NETWORK_EVENT` 保持 2 |
| 可见 SSID 启动关联 + IP | — | `NOT_VERIFIED` | 本次空中没有可见的已保存 AP |
| AP 关掉再恢复的重连 | — | `NOT_VERIFIED` | 需操作者切换 AP；本包未要求为测试改动家庭路由 |
| 错误凭据下的有界回配网 | — | **PARTIAL** | 观察到有界退避，但设备在 36.3 s 已关联、早于板级 60 s 期限，故配网模式未被触达 |
| pin 模式 | — | `NOT_VERIFIED` | 未测，不得写成已验证 |

### 4.3 音频 —— 候选 08 新增行为（CP2）

| 项目 | 证据 | 状态 | 说明 |
| --- | --- | --- | --- |
| int32→int16 归一化正确（无额外缩放/削顶） | 逐窗口验算 `raw_peak >> 16` 与 `peak` | **PASS** | 202 个 ch0 窗口**全部**成立，0 例外，0 个超 int16 上限 |
| 正常音量下无明显削顶 | 同一批窗口 | `NOT_VERIFIED`（有旁证） | 全程 `clipped=0`，最大输入达 ADC 满量程 **78.92%**（`raw_peak=1694695424`）时仍 `clipped=0`；但无受控的正常音量语音刺激，故不判 PASS |
| 单次捕获内 4 次唤醒词 | — | `NOT_VERIFIED` | 操作者两次均无法发声，未施加刺激 |
| 播放期间 INPUT 仍持续产生 | `tx_overlap_n` | `NOT_VERIFIED` | 所有窗口 `tx_overlap_n=0`、`tx_frames=0`，因发送通路从未激活（无按钮点击） |
| 至少两次 3 秒录音/回放 | — | `NOT_VERIFIED` | 无按钮点击 |
| `LOCAL_REFERENCE_PROBE_END drained=1` | — | `NOT_VERIFIED` | BEGIN/END 从未产生 |
| 播放后唤醒恢复 | — | `NOT_VERIFIED` | 无播放周期 |
| 回放可闻 / 听感 | — | `NOT_VERIFIED` | 无播放周期 |

**无刺激的窗口不构成失败率，也不记 `FAIL`。未调整任何增益、阈值或 PGA。**

### 4.4 稳定性 —— 限定烟雾回归（CP3）

修订版 CP3 明确**不重复**候选 05 已有的 20 轮复位与约 31.3 分钟空载；以下为候选 08 自身的三个启动会话检查。

| 项目 | 证据 | 状态 | 说明 |
| --- | --- | --- | --- |
| 崩溃 | 三会话 `abort/panic=0`、`BOOT_BLOCKED=0`、`wdt/canary=0` | **PASS** | 无异常，故无需追加捕获 |
| 启动一致性 | 每会话 `app_main=1`，单次复位，ELF 锚点恒为 `50fb62c30` | **PASS** | 三会话锚点唯一 |
| 内存趋势 | 会话内 free / PSRAM 跨度 512–1040 B，末值不低于首值 | **PASS（限范围）** | 属抖动而非下行趋势；单会话仅 60–100 s，**对小时级漂移无发言权**，且跨会话不可比 |
| 虚假唤醒 | 三会话 `wake` 恒 1、`wakes` 恒 0 | **PASS** | 安静环境下无虚假检测 |
| W/E 级日志 | 每会话 6 行，三会话共 18 行 | **PASS（已逐条归类）** | 2× nv3051f 36h 命令、swap_xy 不支持、mirror 不支持、早于 C5 的 MAC 读取、SDIO 从机复位。**候选 05 启动捕获为完全相同的集合**，续段捕获为 0 行（无启动横幅）。无未分类行 |
| 20 轮复位 / 30 分钟空载 | 候选 05 基线 | **引用，不重跑** | 作为 M0 稳定性基线引用，**不作为候选 08 结果重新声称** |
| 断电冷启动 | — | `NOT_VERIFIED` | USB 复位不是冷启动 |

### 4.5 范围偏差

| ID | 内容 | 处理 |
| --- | --- | --- |
| DEV-08-1 | 一轮 20 次 USB 复位序列在任务书 `6ed2a39` 生效前按旧版 CP3 启动，经用户指示在 round 19 中途停止；18 轮已完成且全部通过，30 分钟空载阶段从未开始 | **排除出验收矩阵**。本地产物已更名为 `cp3-aborted-superseded-scope/`，并在 `cp3-wallclock.txt` 追加中断记录，避免被误读为验收证据 |
| DEV-08-2 | 任务书要求分支 `workbuddy/v6-m0-candidate08-review`（带斜杠）。本机沙箱**无法持久保存 `refs/heads/workbuddy/` 引用**：直接写引用文件后 `git rev-parse --verify` 当场可见、`git symbolic-ref HEAD` 切换后约 3 分钟内工作正常，随后该目录被移除 —— HEAD 变 unborn，`git commit` **返回 exit 0 却不产生提交**（静默假成功） | 改用连字符名 `workbuddy-v6-m0-candidate08-review`，基点与提交内容不变。已从 unborn 状态恢复（`symbolic-ref HEAD` 指回完好分支 + `git reset` 复位被污染的索引），CP4 三个文件全程在盘上未丢，6 笔提交完好。**本机不得再用带斜杠的 workbuddy 分支名** |

---

## 5. 未验证清单（汇总，不得视为通过）

- 音频全部分项：唤醒词、重复唤醒、录音回放、播放期间采集、探针排空、播放后唤醒恢复、回放听感、参考通道。
- 可见 SSID 启动关联与 IP；AP 关掉再恢复的重连；错误凭据下的有界回配网；pin 模式。
- 断电冷启动；小时级稳定性；语音或网络负载下的稳定性。
- SD 卡 / Camera / 电源键。
- 恢复写回（需明文授权，本包未做）。
- NAS / M1；参考通道与 AEC 的有效性。

## 6. 结论

**CODE / HOST / BUILD 三类证据全部通过，无 P0/P1，登记 2 项 P2 观察。**
**DEVICE 类：网络新增分支通过（含隐藏 SSID 重连这一核心修复）；音频归一化经算术独立验证通过，其余音频分项因操作者无法发声记 `NOT_VERIFIED`；稳定性烟雾检查无异常。**

**本矩阵不宣称 M0 总通过，也不构成进入 M1 或其他未授权阶段的依据。**
