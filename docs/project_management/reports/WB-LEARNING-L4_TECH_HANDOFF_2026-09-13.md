# Claw4 学习习惯养成终端 — L4 语音子系统技术交接报告

> **用途**：本报告面向外部协作 AI / 工程师，用于独立研究与方案评估。
> **自包含**：无需项目内部上下文即可阅读。
> **日期**：2026-09-13
> **状态**：L4（语音）主机侧实施完成，但**语音交互方案存在架构级障碍，正在重新选型**。

---

## 0. 一句话结论

设备端已实现"本地唤醒 + 云端 ASR + 学习页确定性语音命令"，**功能链路全部打通**（唤醒、识别、命令派发、事件上报均验证通过）。
但**"云端 ASR → 云端 LLM → 云端 TTS"是一体化流水线**，客户端无法可靠地"只取识别文本、不回话"——导致 LLM 闲聊照播、TTS 被麦克风回采形成自我对话。
**本地命令词方案被分区容量硬否掉**。结论：**建议改用自建服务端（xiaozhi-esp32-server）**，把语音链路控制权收回自己手里。

---

## 1. 项目与硬件背景

| 项 | 值 |
| --- | --- |
| 项目 | Metalio Claw 4 — AI 学习习惯养成终端（面向中小学生） |
| 主控 | **ESP32-P4**（双核 RISC-V，屏幕/触摸/音频/电源） |
| 无线 | **ESP32-C5**（通过 ESP-Hosted SDIO 提供 Wi-Fi） |
| 调试口 | `COM7` = `VID 303A:1001`（USB JTAG/serial），MAC `80:F1:B2:D2:ED:14` |
| 屏幕 | 720×720（NV3051F 类） |
| 固件基线 | 开源项目 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)，上游 commit `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` |
| 构建 | ESP-IDF **v5.5.4**，`riscv32-esp-elf` GCC 14.2.0 |
| 产品目标 | L0 上机 → L1 学习核心 → L2 离线 → L3 网络 → **L4 语音** → L5 MCP → L6 AI 教练 |

### 1.1 分区表（关键约束）

```
nvs        @0x009000   24K
phy_init   @0x110000    4K
model      @0x111000  956K     ← 语音模型分区（srmodels），当前仅放唤醒词
ota_0      @0x200000  9.0M    ← 应用固件所在分区
ota_1      @0xb00000  4.0M    ← 小于固件，无法用于 OTA
```

**关键**：`model`（放语音模型）**只有 956 KB**，且前后紧邻 `phy_init@0x110000` 与 `ota_0@0x200000`，**原地无法扩容**。

---

## 2. 当前固件与代码状态

| 项 | 值 |
| --- | --- |
| 仓库 | `https://github.com/revercgy-hub/claw4-learning-habit-ai` |
| 工作分支 | `workbuddy-app-first-l3-acceptance` |
| 远端 HEAD（报告写作时） | `5d30e49` |
| 固件 | `xiaozhi.bin` = **9,269,392 B** / SHA256 `d9bd2e6c1100aa542cdcb15eaa573541ed745d5cd0c7d9173bdfcba427d280d2` |
| 刷写方式 | `esptool --chip esp32p4 -p COM7 -b 460800 write_flash 0x200000 xiaozhi.bin`（**仅 ota_0 应用分区**） |

> ⚠️ **重要**：上述固件**包含已定位的 D5 缺陷**（见 §5-D5）。另有修复版本已构建但**尚未刷入验证**。

---

## 3. 已完成阶段

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| L0 | 上机闭环（Home → Learning → Mock Task → Start） | ✅ 真机验证 |
| L1 | 学习域核心（任务/会话状态机、NVS 持久化、命令派发） | ✅ |
| L2 | 离线可用（断网操作入本地 outbox，恢复后补传） | ✅ |
| L3 | 网络（设备 → 主机 relay → 后端 API；HMAC 鉴权、事件批量上报、ACK 推进） | ✅ 6/6 验收 |
| **L4** | **语音（本地唤醒 + 云端 ASR + 确定性命令）** | ⚠️ **功能通、方案有架构障碍** |
| L5 / L6 | MCP / AI 教练 | 未开始 |

L3 联调拓扑：

```
设备(192.168.3.49) → TCP 18765 → 主机 relay(192.168.3.26) → backend(127.0.0.1:8000)
```

---

## 4. L4 语音子系统设计

### 4.1 目标

学习页支持**确定性语音命令**（不是自由对话）：

```
开始 / 暂停 / 继续 / 完成 / 跳过 / 还有多久 / 当前任务 / 今天任务
```

### 4.2 实际实现的链路

```
[本地] WakeNet9s 唤醒词（srmodels 分区）
   ↓ 唤醒
[云端] ASR（api.tenclass.net）→ 返回 {"type":"stt","text":"..."}
   ↓ 设备端 application.cc:624 解析
[设备] LVAdapterDisplay::SetChatMessage 路由
   ↓（自定义补丁：学习屏前台时入队）
[设备] LearningScreen::OnVoicePhrase
   ↓ mapVoicePhrase 确定性映射
[设备] dispatcher 派发 或 直接读 state 回答
```

### 4.3 设备端关键代码位置

| 文件 | 作用 |
| --- | --- |
| `main/learning/interaction/stt_mapper.cpp` | 短语 → 命令映射表（精确匹配 + 关键词回退）。**纯函数、无副作用、无 ASR/LLM/TTS** |
| `main/learning/interaction/dispatcher.cpp` | 命令派发。`CommandSource::Voice`；**Voice 来源的"完成"不会直接完成任务，需物理触摸确认** |
| `main/learning/ports/voice_session_port.h` | 抽象端口：`beginSession()` / `endSession()` / `abortCloudReply()` |
| `main/application.cc:624` | 上游 STT 结果处理（`{"type":"stt"}`），当前只喂给聊天屏 |
| `main/application.cc` `SetVoiceUiDesired()` | **语音 UI 会话开关**：开启后唤起唤醒词检测、麦克风归属该会话 |
| `main/audio/audio_service.cc` | 音频喂数循环（wake word `Feed()`）；**D3 缺陷所在** |
| `main/display/lv_adapter_display.cc` | `SetChatMessage` 显示路由；**D4 缺陷所在** |
| `main/display/screen/learning_screen/learning_screen.cc` | 学习屏（本项目新增），**D4/D5/D6 缺陷所在** |

### 4.4 上游语音链路的关键事实（踩坑得出）

1. **唤醒词模型在独立分区**：`srmodels`（`model` 分区 `0x111000`）。**只刷 `ota_0` 不会更新唤醒词**。
2. **唤醒词音频也会被送去云端 ASR**：返回的第一段文本就是唤醒词本身（实测 `"Hi 钛灵"`）。**不能把它当作"未匹配命令"就中断会话**，否则永远收不到后续真实命令。
3. **`AbortSpeaking()` 只发协议消息给云端**（`{"type":"abort"}`），**不清理设备本地已缓冲的 TTS**。
4. **唤醒词只在"语音 UI 会话"内生效**：聊天页/数字人页之外（如学习页）必须显式获取会话。

---

## 5. 已定位缺陷清单（含证据）

> 这是本报告的核心。**D3 / D4 / D5 三个都是"设备整机卡死或重启"级别的严重缺陷**，且都在 L4 实施过程中被触发暴露。

### D1 — 增量构建漏编（已修复）

**现象**：源码里有 `pause_count` 字段，设备上报的事件里却没有。

**根因**：镜像同步时**保留了源文件旧 mtime**（09:27），ninja 判定"源不比目标（18:44）新"→ **跳过重编** → 固件里根本没有该字段。

**排错要点（重要）**：
```
.obj 文件里 grep 到字符串 ≠ 它是被编译进去的字面量！
必须做「段归属分析」：
  objdump -h <obj>            # 取各段 file offset 区间
  grep -abo '<字符串>' <obj>  # 取命中偏移
  落在 .debug_str/.debug_info → 只是 DWARF 调试信息里的成员名（假阳性）
  落在 .rodata.*              → 才是真字面量
```
实测：`pause_count` 在偏移 412647，落在 `.debug_str`；而 `actual_seconds` 在偏移 10396，落在 `.rodata`。

**修复**：构建前 `touch` 全部业务源文件（385 个），原构建目录增量重建。
**验证**：字面量 0→1 + 镜像段级对比（仅 rodata +256 B）+ `sdkconfig` md5 未变。

---

### D2 — `ResetToSeed()` 导致事件序号永久失配（未修复）

**现象**：设备端连续多轮上报，后端一律 `rejected=5`、`ack` 卡死不动；事件**永远无法入库**。

**根因链**：
```cpp
// learning_runtime.cpp:65
bool LearningRuntime::ResetToSeed() {
  backend_.reset();
  if (!NvsOutboxStorage::eraseAll()) { ... }   // ← 把持久化的 next_sequence 一起擦了
  app_ = std::make_unique<LearningApp>(storage_, clock_);
  app_->applyTodaySnapshot(DemoTodaySnapshot());  // ← 还用 demo 快照覆盖后端下发的今日任务
```
```cpp
// outbox_core.cpp:198
// loadState() 失败 → next_sequence 回退为 1
```
→ 设备发出 `seq 1..5`，而后端 `last_acked_sequence = 20`（期望 21）→ 判 **`sequence_regression`** → 全拒 → **不会自愈**（重试同样被拒）。

**已做的临时绕过**（非修复）：备份 DB → 清空 `events` 表 → `last_acked_sequence = 0`。

**建议修复方向**：
- 方案 A：重置后按后端 ack 重新起算序号
- 方案 B：设备端按 batch 响应自愈（收到 ack 后校正本地序号）
- 方案 C（不推荐）：后端放宽 sequence 校验 —— **会破坏事件幂等性**

---

### D3 — `audio_input` 忙循环饿死 IDLE0（已修复）

**现象**：语音会话开启后，`task_wdt` 每 10 秒触发一次，整机卡死。

```
E task_wdt: Task watchdog got triggered.
E task_wdt:  - IDLE0 (CPU 0)
E task_wdt: Tasks currently running:
E task_wdt: CPU 0: audio_input        ← 元凶
```

**根因**：
```cpp
// main/audio/audio_service.cc（wake word 喂数循环）
while (running) {
    samples = GetFeedSize();
    if (samples > 0) {
        std::vector<int16_t> data;
        if (ReadAudioData(data, 16000, samples)) {
            std::lock_guard<std::mutex> lock(wake_word_mutex_);
            if (wake_word_initialized_ && wake_word_) {
                wake_word_->Feed(data);
            }
            continue;          // ← 无任何让步点！
        }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
```
当 AFE ringbuffer 满（日志 `AFE: Ringbuffer of AFE(FEED) is full`，实测刷了 **8733 次**）时，
`Feed()` 变成**非阻塞立即返回** → 循环再无障碍 → **忙等占满 CPU0** → IDLE0 饿死 → 看门狗。

**修复**：
```cpp
if (ReadAudioData(data, 16000, samples)) {
    {
        std::lock_guard<std::mutex> lock(wake_word_mutex_);   // 锁作用域收紧
        if (wake_word_initialized_ && wake_word_) {
            wake_word_->Feed(data);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1));   // 让步点
    continue;
}
```
**验证**：`task_wdt` 8733 次 → **0**。

---

### D4 — 跨任务操作 LVGL 导致重绘死循环（已修复）

**现象**：语音命令**成功执行后**约 10 秒，`task_wdt` 开始刷，形态变为：

```
E task_wdt:  - IDLE1 (CPU 1)
E task_wdt: CPU 0: IDLE0
E task_wdt: CPU 1: main_event_loop     ← 元凶换了
```

**定位方法（可复用）**：看门狗会打印 `Print CPU1 backtrace`，用 `addr2line` 符号化后：

```
lv_inv_area                 lvgl/src/core/lv_refr.c:277
lv_obj_invalidate_area      lvgl/src/core/lv_obj_pos.c:848
lv_obj_invalidate           lvgl/src/core/lv_obj_pos.c:864
lv_label_refr_text          lvgl/src/widgets/label/lv_label.c:1210
RefreshUi()                 learning_screen.cc:252      ← 落到业务代码
```
（栈尾的 `UpgradeFirmware`、`_Deque_iterator` 等是误解析噪声，**前 3~5 帧才是有效调用链**。）

**根因**：显示层补丁**插在了上游 LVGL 锁的外面**：

```cpp
// lv_adapter_display.cc :: SetChatMessage
const bool learn_active = LearningScreen::IsActive();
if (learn_active) {
    if (is_user) LearningScreen::OnVoicePhrase(content);   // ← 补丁在锁外
    return;
}
if (!chat_active && !dp_active) return;

if (esp_lv_adapter_lock(-1) != ESP_OK) return;   // ★ 上游的 LVGL 锁在这里
if (chat_active) { ChatScreen::AddMessage(...); }  // 上游自己都在锁内
```

`SetChatMessage` 由**协议任务**调用，而 `OnVoicePhrase` 内部有 `lv_label_set_text` / `lv_timer_del`。
**LVGL 不是线程安全的** → 与 LVGL 任务里的 `RefreshUi()` 并发改动同一批对象
→ 失效区链表损坏 → `lv_inv_area` 遍历死循环 → 饿死 IDLE1。

**修复（不要简单挪进锁内）**：因为 `OnVoicePhrase` 还要发协议消息（`abortCloudReply`），
**在 LVGL 锁内发协议消息有锁顺序反转风险**。改为**消息传递**：

```cpp
// 协议任务：只入队（仅 std::mutex + deque，绝不碰 lv_*）
if (learn_active) {
    if (is_user) LearningScreen::QueueVoicePhrase(content);
    return;
}

// 学习屏：50 ms 的 LVGL 定时器，在 LVGL 任务上下文取出执行
void OnVoicePollTick(lv_timer_t*) {
  for (;;) {
    std::string phrase;
    { std::lock_guard<std::mutex> lk(s_voice_q_mtx); if (s_voice_q.empty()) break;
      phrase = std::move(s_voice_q.front()); s_voice_q.pop_front(); }
    LearningScreen::OnVoicePhrase(phrase.c_str());
  }
}
```

**通用铁律**：任何由**协议任务 / 网络任务 / 事件循环**回调进来的显示层入口（`SetChatMessage`、`SetEmotion`…），
**一律不得直接调用 `lv_*`**。

---

### D5 — `lv_timer` 双重删除 → 堆破坏 → 断言重启（已修复，待真机验证）

**现象**（最严重）：

```
assert failed: xQueueSemaphoreTake queue.c:1713 (pxQueue->uxItemSize == 0)
ESP-ROM:esp32p4-eco2-20240710
rst:0xc (SW_CPU_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
```
设备**自动重启**。注意：**过程中没有 `task_wdt`**——因为是死锁/断言，不是忙循环。

**根因**：
```cpp
s_voice_timer = lv_timer_create(OnVoiceWindowTimeout, kVoiceWindowMs, nullptr);
lv_timer_set_repeat_count(s_voice_timer, 1);   // ← LVGL 在到期后【自动删除】该 timer
...
// CloseVoiceWindow() / ArmVoiceWindow() 里又：
lv_timer_del(s_voice_timer);                   // ← 对已删除的指针再删 = use-after-free
```

**时间线完全吻合**：
| uptime | 事件 |
| --- | --- |
| 497 s | 点「语音」→ 创建 15 s 单次 timer |
| **512 s** | **timer 到期，LVGL 自动删除** → `s_voice_timer` 成为悬空指针 |
| 514 s | 命令命中 → `CloseVoiceWindow` → `lv_timer_del(悬空)` → **第 1 次堆破坏** |
| 570 s | 第二次会话 |
| 592 s | 再次命中 → **第 2 次堆破坏** |
| ~632 s | `xQueueSemaphoreTake` 拿到损坏句柄 → **assert → SW_CPU_RESET** |

**修复**：**彻底不用一次性 timer**。改为在既有的 50 ms 轮询定时器里比较 `lv_tick_get()` 时间戳，
**全程不新增、不删除任何 timer**：

```cpp
uint32_t s_voice_deadline = 0;   // 0 = 窗口未开启
// ArmVoiceWindow():  s_voice_deadline = lv_tick_get() + kVoiceWindowMs;
// CloseVoiceWindow(): s_voice_deadline = 0;
// OnVoicePollTick() 尾部：
if (s_voice_open && s_voice_deadline != 0 &&
    static_cast<int32_t>(lv_tick_get() - s_voice_deadline) >= 0) {
    CloseVoiceWindow("语音已关闭（15 秒未识别）");
}
```

**教训**：`lv_timer_set_repeat_count(t, 1)` 与手动 `lv_timer_del()` **不可混用**。
安全模式是「**无限重复 + 单点删除**」（项目里其他 timer 都是这个模式，从未出问题）。

---

### D6 — 语音时间窗策略缺陷（已修复）

**现象**：`voice kind=` 计数为 **0** —— 一个学习命令都没收到。

**根因**：上游会把**唤醒词音频也做一次 ASR**，返回文本形如 `"Hi 钛灵"`。
而早期实现在 `OnVoicePhrase` 里**一进来就 `abortCloudReply()`，且"未识别"分支也关闭时间窗**
→ **把刚建立的对话流程在第一步就掐死** → 用户后面说的命令根本没机会上来。

**修复**：
| 改动 | 说明 |
| --- | --- |
| 「未识别」分支**不再掐断、不再关麦** | 唤醒词回声不摧毁会话 |
| `abortCloudReply()` **移到"命中学习命令"之后** | 只在真正识别到命令时才掐断 |
| 时间窗 8 s → **15 s** | 需覆盖「唤醒 + 说命令 + ASR 往返」 |
| 收到任何语音文本即**续期窗口** | 避免还没说出命令就被关麦 |

---

### D7 — 唤醒词不匹配（未修复）

**现象**：对设备说「你好小智」**毫无反应**。

**根因**：
```
I AfeAwakeWord: Model 0: wn9l_hai1tai4ling2      ← 设备加载的是「嗨钛灵」
```
而固件配置写的是「你好小智」（`CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y`）。
**唤醒词模型存放在独立的 `srmodels` 分区**，本项目一贯只刷 `ota_0`，**未刷过 srmodels** → 设备上仍是旧模型。

**修复方式**：刷 `srmodels` 分区（`0x111000`，约 292 KB）。**这是 ota_0 之外的分区刷写，需单独授权。**

---

### D8 — L3 网络超时阻塞应用主循环（未修复，但影响 L4）

**现象**：进入聊天/学习页后，唤醒词要 **18 秒**才真正启用，用户早已离开页面。

```
I (448501) voice UI desired -> 1 (epoch=7)
I (448501) ChatScreen: load -> schedule voice UI start
   ...18 秒内应用几乎不执行任何任务...
E (466835) EspTcp: Failed to connect to 192.168.3.26:18765, code=0x71   ← 21 秒超时
I (466835) Application: ApplyVoiceUiStart      ← 超时结束后才轮到执行
I (466905) Wake word AFE created
I (467706) voice UI desired -> 0               ← 0.8 秒后用户已离开页面
W ToggleChatState ignored: voice UI session inactive   ← 连电源键都被忽略
```

**根因**：L3 的后台同步 worker 每 15 秒发一次 HTTP 请求，**网络不通时阻塞主循环约 21 秒**，
导致 `SetVoiceUiDesired(true)` 之后要做的 AFE 启动任务全部排队等待。

**注意**：这是 L3 遗留问题，**但它会拖垮任何实时功能（语音、动画）**，必须修。

---

## 6. 架构级问题（核心，决定方案可行性）

> 以下不是"bug"，是**所选技术路线的固有代价**，也是**建议更换方案的根本原因**。

### A1 — 云端 ASR 与 LLM/TTS 是不可分割的一体流水线

上游（xiaozhi-esp32 官方服务）的处理是：

```
音频 → ASR → 文本 → LLM 生成回复 → TTS 合成 → 下发设备播放
```

设备端**只能在收到 ASR 文本后做事后干预**，而**无法告诉服务端"这条只识别、不要生成回复"**。

**后果**：孩子在设备上说「暂停」，设备除了执行暂停，**还会用 TTS 播报一句 LLM 闲聊**（实测听到 `"好的，开始啦。"`、`"你想让我做什么呢？"`）。

### A2 — `AbortSpeaking()` 是"尽力而为"，做不到静默

```cpp
void Application::AbortSpeaking(AbortReason reason) {
    aborted_ = true;
    if (protocol_) protocol_->SendAbortSpeaking(reason);   // 只发 {"type":"abort"} 给云端
}
```
它**不清理设备本地已缓冲的 TTS 音频**。且上游是服务端流水线，设备发 abort 时服务端可能**已经在生成或下发语音**。
**实测：掐断调用确实发生了（日志有 `cloud reply aborted`），但 LLM 回复照播。**

> 早期判断"命中即掐断能避免闲聊"**过于乐观**。实际只能减少，做不到静默。

### A3 — TTS 回声被麦克风回采，形成自我对话（最致命）

**实测日志**（设备自己的播报被 ASR 识别成用户输入）：

```
I Application: << 你好呀！                          ← 设备播放的 TTS
I Application: >> 你好呀有。                        ← 麦克风回采后再识别
I LearningScreen: voice phrase not a learning command (session kept): 你好呀有。
I Application: << 你想聊什么话题呢？
I Application: >> 以想聊聊天。                      ← 又是回采
```

**后果**：
1. 用户的真实命令被自己的 TTS 回声**淹没**
2. 设备陷入**自我对话循环**
3. 这也是"只能识别到第一个命令、后面就没反应"的**直接原因**

> 上游聊天场景由 `AEC(VOIP_HIGH_PERF)` + 会话状态机抑制，但**学习页复用该会话时无法可靠抑制**。

### A4 — 儿童语音上传第三方服务

云端 ASR 走 `api.tenclass.net`（十方融海）。**儿童语音离开家庭网络**。
（本项目已获用户明确授权，但从产品与合规角度是长期风险。）

### A5 — 本地命令词方案被分区容量硬否掉

最理想的方案是**本地命令词识别**（ESP-SR MultiNet）：零外传、离线可用、天然不触发 LLM。

**但客观约束**：

| 项 | 值 |
| --- | --- |
| MultiNet 中文模型 `mn7_cn_data` 大小 | **2.66 MB** |
| `model` 分区容量 | **956 KB**（已用 292 KB 放唤醒词） |
| 余量 | **约 670 KB** —— 差 **4 倍** |
| 能否原地扩容 | **不能**：前后紧邻 `phy_init@0x110000` 与 `ota_0@0x200000` |

要放得下就必须**移动 `ota_0` 起始地址**（分区表大改）→ 破坏现有回滚基线，风险最高。

---

## 7. 已尝试方案与失败原因（决策轨迹）

| # | 方案 | 结果 | 失败原因 |
| --- | --- | --- | --- |
| 1 | **本地命令词**（ESP-SR MultiNet） | ❌ 否决 | 模型 2.66 MB vs 分区 956 KB，且分区无法原地扩容 |
| 2 | **云端 ASR + 客户端"命中即掐断"** | ⚠️ 功能通、体验不达标 | LLM 回复照播（A2）；TTS 回声回采形成自我对话（A3） |
| 3 | **改分区表塞本地模型** | ❌ 未采纳 | 需移动 `ota_0` 起始地址，破坏回滚基线，风险最高 |
| 4 | **自建服务端** | ⏳ **待评估（推荐）** | 见 §8 |

> 说明：结论三次变化都是**被新证据推着走**（隐私 → LLM 副作用 → 分区容量 → 回声回采），不是反复。

---

## 8. 建议方向：NAS 自建服务端（xiaozhi-esp32-server）

**项目**：[xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server)
（华南理工大学刘思源教授团队主导；为 xiaozhi-esp32 提供自建后端，Python/Java/Vue）

### 8.1 为什么它能解决上述问题

| 问题 | 自建服务端如何解决 |
| --- | --- |
| **A1** 无法"只识别不回复" | **服务端逻辑完全可控**。可自定义意图/插件：识别到学习命令 → **直接返回、不调 LLM、不下发 TTS** |
| **A2** 掐断不可靠 | 不需要掐断——**服务端根本不生成**那段语音 |
| **A3** TTS 回声回采 | 可调服务端 VAD / 打断策略 / 播放期闭麦；也可在学习模式下限定只在唤醒后开窗 |
| **A4** 隐私 | **ASR/TTS/LLM 全可本地**：FunASR / SherpaASR + Index-TTS / FishSpeech + Ollama。**儿童语音不出内网** |
| **A5** 分区容量 | 不需要动分区——识别在服务端做 |

### 8.2 关键能力（已核实）

- **协议**：WebSocket + MQTT+UDP，**兼容 xiaozhi-esp32 设备**（提供 OTA 接口与 WebSocket 接口地址）
- **ASR 本地**：FunASR、SherpaASR（免费、可离线）
- **TTS 本地**：FishSpeech、GPT-SOVITS V2/V3、Index-TTS、PaddleSpeech（免费）
- **LLM 本地**：Ollama 接口
- **VAD**：SileroVAD（本地）
- **意图识别**：`function_call` / `intent_llm` / **`nointent`（不进行意图识别，直接返回对话结果）**
- **插件系统**：支持自定义插件开发与**热加载**
- **部署**：Docker（最简版 / 全模块版）。用 FunASR 建议 **4 核 8G**（全模块）或 **2 核 4G**（最简版）

### 8.3 设备侧需要改什么

把设备指向自建服务：**OTA 地址 + WebSocket 地址**（当前指向官方 `api.tenclass.net`）。
需确认这两处在固件中的配置方式（编译期宏 或 NVS 运行时可配）。

### 8.4 待验证假设

1. **服务端能否在不改客户端的情况下实现"命中命令 → 静默执行"**（关键）
2. `nointent` 模式 vs 自定义插件，哪个更适合"学习模式"
3. 设备固件改服务器地址的最小改动路径与工作量
4. FunASR / SherpaASR 对**儿童中文语音**的识别率
5. NAS 规格是否够（FunASR 与 TTS 同时常驻的内存占用）

---

## 9. 复现与诊断手册

### 9.1 构建（绕过 `idf.py`，直接 ninja 时必备环境）

```bash
# 缺环境变量时的典型报错：
# kconfiglib.core.KconfigError: couldn't parse 'depends on $(ESP_IDF_VERSION) >= "6.1"'
#   → 这是环境缺失，不是代码问题。

export IDF_PATH=E:/workbuddy/esp-idf-5.5.4-ascii     # 必须与 build.ninja 一致
export ESP_IDF_VERSION=5.5
export IDF_VERSION=5.5.4
export IDF_TOOLS_PATH=E:/workbuddy/claw4-idf-tools
export IDF_PYTHON_ENV_PATH=$IDF_TOOLS_PATH/python_env/idf5.5_py3.12_env
unset PYTHONPATH
# 构建目录：E:/workbuddy/claw4-idf-cold-c5-20260906（CMAKE_HOME_DIRECTORY=E:/c）
# 现有脚本：C:/Users/rever/AppData/Local/Temp/claw4_rebuild.sh
```

**构建前必须 `touch` 源文件**（否则触发 D1 漏编）。

### 9.2 固件验证三种手段

```bash
# ① 字面量（日志文本、协议字段名）—— 只能验证字符串
grep -ac "pause_count" xiaozhi.bin

# ② 函数符号 —— 验证新增/改名的函数（grep .bin 查不到是正常的！）
riscv32-esp-elf-nm -C xiaozhi.elf | grep -E "QueueVoicePhrase|OnVoicePollTick"

# ③ 运行时崩溃定位 —— 看门狗/panic 的 backtrace
riscv32-esp-elf-addr2line -f -C -e xiaozhi.elf 0x4828ecfa 0x482876c6 ...
#   注意：前 3~5 帧有效，其后多为栈噪声（会出现明显不相关的函数名）
```

### 9.3 设备回读（只读，不写不擦）

```bash
esptool --chip esp32p4 -p COM7 -b 460800 read_flash 0x200000 0x2000 head.bin
# app 描述符 magic = 0xABCD5432；elf_sha256 在 magic+144
```

### 9.4 已知环境陷阱

| 陷阱 | 说明 |
| --- | --- |
| **串口打开即复位** | `serial.Serial('COM7')` 拉 DTR 会让 ESP32-P4 当场重启；排查时勿反复开关 |
| **捕获进程独占串口** | 会挡住刷机（`PermissionError(13)`），需先杀进程；`wmic` 在新版 Windows 已移除，用 ctypes 遍历 |
| **数据线** | 设备 USB 会偶发掉线（PnP 全 `CM_PROB_PHANTOM`），重插恢复 |
| **I2C 总线锁死** | GT911/TCA95xx/BQ27220/LCD 同时报错，**软复位无效，必须彻底断电** |
| **局域网被劫持** | 本机装了 Tailscale，其 exit-node + 子网路由会劫持 `192.168.3.0/24`；判据：`route print` 里下一跳是 `100.100.100.100` → `tailscale down` |
| **设备时间戳** | 为 **uptime 秒**（无 SNTP），家长端时间轴会失真 |

---

## 10. 待研究问题（求助清单）

**优先级 P0（决定方案走向）**

1. 在 xiaozhi-esp32-server 上，**如何实现"识别到指定词 → 只执行、不调 LLM、不下发 TTS"**？
   `nointent` 模式够用吗，还是必须写自定义插件？请给出具体配置或代码路径。
2. 设备端如何最小改动地**切换到自建服务器**（OTA + WebSocket 地址）？是否支持 NVS 运行时配置？
3. 是否有成熟做法**彻底消除 TTS 回声回采**（服务端 VAD / 播放期闭麦 / 服务端打断）？

**优先级 P1**

4. FunASR / SherpaASR 对**中文儿童语音**（尤其小学生口音）的识别率与资源占用实测？
5. NAS 配置评估：同时跑 ASR + TTS + LLM（Ollama），**最低可用规格**是多少？
6. 学习页"按需开麦时间窗"（当前 15 秒）与语音体验如何平衡？有没有更好的交互（按住说话 / 唤醒词后限定下一句）？

**优先级 P2**

7. 是否值得**改分区表**把本地命令词模型塞进去？（收益：零外传 + 零 LLM 干扰；代价：破坏回滚基线）
8. D2（ResetToSeed 序号失配）的最佳修复方案：设备端自愈 vs 重置后重新起算？

---

## 11. 附录：项目仓库

| 项 | 值 |
| --- | --- |
| 仓库 | `https://github.com/revercgy-hub/claw4-learning-habit-ai` |
| 分支 | `workbuddy-app-first-l3-acceptance` |
| 相关报告 | `docs/project_management/reports/` 目录 |
| 任务看板 | `docs/project_management/TASK_BOARD.md`（含 6.31~6.33 缺陷登记） |
| 集成补丁登记 | `integration/metalio_claw4/integration_manifest.md`（6 条上游补丁） |

**上游补丁清单**（本项目对官方 xiaozhi-esp32 的最小改动，均可单文件回滚）：

| # | 文件 | 内容 |
| --- | --- | --- |
| 1 | `main/display/screen/home_screen/home_screen.cc` | Home 注册 Learning App |
| 2~4 | `main/CMakeLists.txt` | 源文件登记（learning 组件 / 语音会话端口） |
| 5 | `main/display/lv_adapter_display.cc` | STT 文本路由到学习屏（**D4 相关**） |
| 6 | `main/audio/audio_service.cc` | 喂数循环让步点（**D3 修复**） |
