# V6 M0 唤醒词插桩提案（给 Codex）

> Codex 复检修正：P2/P3 采纳；P1 回调已接，但 wake-only 模式不产生 VAD，不能按原判据解释。录放音绕过 AFE，原“依赖 OnOutput”推论不成立。实施双通道电平统计，保持增益不变。详见 CODEX_V6_M0_REVIEW_2026-09-22.md。

状态：**原提案已由 Candidate13 实施并实机复核，本文后续章节保留为历史设计记录，不是当前工作项**。P1 VAD 回调、P2 wake rearm、P3 事件计数和 P4 双通道 RMS/peak 插桩均已进入 Candidate13。2026-09-23 实测四次低音量唤醒词有两次命中且均 rearm；但输入参考通道 ch1 在所有窗口保持全零，安静回放期间 VAD 触发两次，AEC/参考链路现在是明确的修复门禁。当前证据与后续工作见 [`V6_M0_CANDIDATE13_REPORT.md`](V6_M0_CANDIDATE13_REPORT.md)。

下列提案文字记录 Candidate13 实施前的假设与建议；其中“尚未观测 VAD / 电平”的表述已被后续插桩和实测取代。不要据此推断 AEC 已通过。
背景证据：`WB-V6-M0-AUDIT_2026-09-22.md` §14、`V6_M0_NETWORK_PROBE.md`。
目标文件（唯一）：`integration/v6/board/claw4-learning-v6/m0_diagnostics.cc`
（当前内容哈希经 LF 归一化后 `0a090868223bbb1499c30e2877250f7bcc79479578eb8fc89db089f2f5a7f141`，
与 `m0-candidate-04.json` 记录一致，与 `E:/v6/s1` 构建树副本内容一致，仅行尾差异。）

---

## 1. 已经确证的事实（不要重复验证）

| # | 事实 | 证据 |
| --- | --- | --- |
| 1 | 设备上的唤醒词模型**正确** | 回读 `0x10f000` 前 291,042 B，SHA256 `7c87dd7a…` 与 `build/srmodels/srmodels.bin` **逐字节相同**；内含 `wn9_nihaoxiaozhi_tts` 与字面量 `你好小智` |
| 2 | 检测器**处于工作态** | 复位后 105 s 内 HEALTH `wake=1` 全程成立（`IsWakeWordRunning()`） |
| 3 | 麦克风音频**确实到达 AFE 并产出** | 按屏幕按钮的 3 s 采集 + 回放，用户确认可闻；而该回环依赖 `audio_engine_->OnOutput()` → 编码队列 → `audio_testing_queue_`，**引擎必须先被 feed 才可能有输出** |
| 4 | 语音**没触发** | 复位后 105 s、约 10 次正确发音、正对设备，`WAKE_DETECTED` = 0 |
| 5 | 但**能触发过** | 复位前那一轮 `wake=0`（锁存位被 `audio_service.cc:97` 清掉），而 `taps=0`、`Start()` 只调过一次 ⇒ 只能是一次真实命中，时间线指向用户当时打的响指（误触发） |

**⇒ 已排除**：模型缺失/错配、检测器未启动、麦克风完全不工作、通道被整体接反（否则回环不会可闻）。

---

## 2. 剩下的唯一关键未知

**到达 WakeNet 的信号电平是否足够触发唤醒阈值。**

回环可闻只证明"有信号"，**不证明电平足够**：回环是直通放大，阈值判定是另一回事。
而当前**没有任何电平或 VAD 的可观测点** —— 这是唯一的拦路问题。

---

## 3. 提案：三处插桩（全部在 `m0_diagnostics.cc`，不动上游/板级逻辑）

### P1（核心）接上 VAD 回调 —— 这是判定的关键探针

`AudioServiceCallbacks::on_vad_change` 存在（`audio_service.h:85`，`std::function<void(bool)>`），
上游 `application.cc:84` 已经接了它，**只有 M0 脚手架没接**。

有了它，下一轮就能把"电平够不够"与"唤醒词能不能识别"分开：

| VAD 是否随说话变化 | `WAKE_DETECTED` | 结论 |
| --- | --- | --- |
| 随说话 0/1 跳变 | 出现 | **唤醒词 PASS** |
| 随说话 0/1 跳变 | 不出现 | 电平够 ⇒ 问题在唤醒通道（阈值/模型与现场音色）→ 交 Codex 调 AFE 阈值 |
| 始终为 0 | 不出现 | **电平不足，或 AEC 把用户声音当回声消掉** ⇒ 查 §4 |

### P2 修自恢复缺陷

唤醒回调会清 `AS_EVENT_WAKE_WORD_RUNNING`（`audio_service.cc:97`），而音频输入任务
**只在位被置位时才 `Feed()`**，空闲循环又不重新置位 ⇒ **一次命中之后永久停机**。
后果：不做这步，下一轮仍然只能测到"第一次"。

安全性已核实：`AudioService::InitializeAudioEngine()` 是幂等的
（`if (audio_engine_initialized_) return true;`，`audio_service.cc:871-873`），
重复调用 `EnableWakeWordDetection(true)` 不会重建引擎。

### P3 加计数

当前只有一次性日志且位清零不可逆 ⇒ **命中次数根本无法统计**。
把 `wake` / `vad` 计数并进 HEALTH，才谈得上"命中率"。

---

## 4. 硬件怀疑（需要一次量化实测，**不要凭推理改**）

`claw4_audio.cc:70`：

```cpp
dest[total + i] = static_cast<int16_t>(std::clamp<int32_t>(buffer[i] >> 12, -32768, 32767));
// Preserve the source board's ADC alignment/gain, verify both channels on hardware.
```

**Codex 自己已注明"需要在硬件上验证"**，这个前置条件至今没有兑现。算术上的两种可能：

| 若 ADC 实际格式 | `>> 12` 的后果 |
| --- | --- |
| 24 bit **左对齐**在 32 bit 槽 | 正确应为 `>> 16`；`>> 12` **过冲 16 倍** → 削顶失真 |
| 24 bit **右对齐**在 32 bit 槽 | 只到 int16 满量程的约 6%（≈ −24 dB）→ **过轻，可能低于 WakeNet 阈值** |

两者对回环的主观影响不同（失真 vs 偏轻），但对 WakeNet 阈值的影响都很直接。
**只改 gain 而不先量电平，是拿猜替换猜。**

### P4（可选但最省事）加一条 rms/peak 日志

在 `Claw4Audio::Read` 或 AFE feed 处每秒打印一次 int16 的 rms 与 peak。
有了这个数字，电平问题当场定论，不需要再靠间接推理。

---

## 5. 建议的补丁（可直接应用；仅改一个文件）

```diff
--- a/integration/v6/board/claw4-learning-v6/m0_diagnostics.cc
+++ b/integration/v6/board/claw4-learning-v6/m0_diagnostics.cc
@@ -18,6 +18,9 @@
 static std::atomic<bool> record_requested{false};
 static std::atomic<unsigned> taps{0};
+static std::atomic<unsigned> wake_events{0};
+static std::atomic<unsigned> vad_events{0};
+static std::atomic<int> vad_state{0};
 
 extern "C" void app_main() {
@@ -34,7 +37,14 @@
     AudioServiceCallbacks callbacks;
-    callbacks.on_wake_word_detected = [](const std::string&) { ESP_LOGI("V6M0", "WAKE_DETECTED (local only)"); };
+    callbacks.on_wake_word_detected = [](const std::string&) {
+        const unsigned n = wake_events.fetch_add(1) + 1;
+        ESP_LOGI("V6M0", "WAKE_DETECTED (local only) count=%u", n);
+    };
+    // P1: the M0 harness never observed VAD, so "is speech reaching the AFE
+    // above the detection floor" was unanswerable. This makes it observable.
+    callbacks.on_vad_change = [](bool speaking) {
+        vad_state.store(speaking ? 1 : 0);
+        if (speaking) vad_events.fetch_add(1);
+        ESP_LOGI("V6M0", "VAD speaking=%d count=%u", speaking ? 1 : 0, vad_events.load());
+    };
     audio.SetCallbacks(callbacks);
     audio.EnableWakeWordDetection(true);
@@ -67,11 +77,21 @@
         }
+        // P2: the wake callback clears AS_EVENT_WAKE_WORD_RUNNING and the audio input
+        // task only feeds the engine while that bit is set, so without re-arming here
+        // the first detection silently disables every later one. Safe to repeat:
+        // AudioService::InitializeAudioEngine() returns early once initialised.
+        if (!audio.IsWakeWordRunning()) {
+            audio.EnableWakeWordDetection(true);
+            ESP_LOGI("V6M0", "WAKE_REARM");
+        }
         if (++ticks % 10 == 0) {
-            ESP_LOGI("V6M0", "HEALTH free=%u psram=%u wake=%d taps=%u",
+            ESP_LOGI("V6M0", "HEALTH free=%u psram=%u wake=%d taps=%u wakes=%u vads=%u vad=%d",
                      unsigned(esp_get_free_heap_size()),
                      unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
-                     audio.IsWakeWordRunning(), taps.load());
+                     audio.IsWakeWordRunning(), taps.load(),
+                     wake_events.load(), vad_events.load(), vad_state.load());
         }
```

**兼容性**：`hw_matrix.py` 的 HEALTH 正则为非锚定匹配（`V6M0: HEALTH free=(\d+) psram=(\d+) wake=(\d+) taps=(\d+)`），
追加字段不会破坏解析。插桩落地后我会把 `wakes=` / `vads=` / `vad=` 加进信号表并补测试。

**范围**：只改 `m0_diagnostics.cc`；不动上游 `main/audio/`、不动 `claw4_audio.cc`、不动 `sdkconfig`、
不动分区表。需重新构建 + 冻结新候选 + 记录新 overlay 哈希。

---

## 6. 下一轮真机验证的判据（插桩落地后）

```bash
# 复位后 105 s（60 + 45），用户正对设备、约 20 cm、正常偏大音量说 4 次「你好小智」
$PY tools/v6/capture_device.py --port COM7 --seconds 60 --reset --output <绝对路径>/wake-instr-01.txt
$PY tools/v6/capture_device.py --port COM7 --seconds 45            --output <绝对路径>/wake-instr-02.txt
```

| 观测 | 结论 |
| --- | --- |
| `VAD speaking=1` 出现 ≥ 1 次，且 `WAKE_DETECTED count=1` | 唤醒词 **PASS** |
| `VAD speaking=1` 出现 ≥ 1 次，`WAKE_DETECTED` 为 0 | 电平够 → 转阈值/模型问题 |
| `VAD speaking=` 从不出现，或全为 0 | 电平不足或 AEC 消声 → 回到 §4，先量 rms |
| `WAKE_REARM` 出现且其后 `WAKE_DETECTED count` 递增 | P2 生效，可统计多次命中 |

---

## 7. 需要用户配合 / 需要 Codex 决策

| 项 | 谁 |
| --- | --- |
| 确认 P1/P2/P3 是否采纳、是否加 P4 的 rms 插桩 | Codex |
| 改 `m0_diagnostics.cc` + 重新构建 + 冻结候选 + 刷机 | Codex（真机刷写仍需用户当次授权范围） |
| 下一轮真机测试时正对设备说唤醒词 | 用户 |
| 若走到 §4，决定是否动 `claw4_audio.cc` 的 gain | Codex，且**必须先有 rms 数据** |
