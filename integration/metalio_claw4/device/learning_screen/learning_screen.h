// claw4 Learning App screen (device build tree).
// WB-LEARNING-V4-L1 / L4 — renders REAL domain state driven by the host-verified
// funnel (LearningApp over NVS storage + esp_timer clock). L3 wired the backend
// session; L4 (P25) adds deterministic VOICE commands.
//
// Voice path (L4):
//   display 层在「学习屏前台 + 收到 user 识别文本」时调用 QueueVoicePhrase()
//   （跨任务安全，仅入队）；学习屏的 LVGL 定时器在 LVGL 任务上下文取出后调用
//   OnVoicePhrase() 做确定性映射（mapVoicePhrase），命中即派发并掐断云端回复，
//   未命中给出支持短语提示 —— 学习屏不做自由对话。
#pragma once

#include "lvgl.h"
#include "screen_util.h"

class LearningScreen {
 public:
  static lv_obj_t* Create();
  static void LifecycleCallback(screen_lifecycle_event_t event);

  // L4 (P25 Voice STT)：是否处于前台。display 层据此决定是否把语音识别文本
  // 路由给学习屏（否则按上游策略交给聊天/数字人屏或丢弃）。
  static bool IsActive();

  // L4：语音识别文本入口。执行 mapVoicePhrase 确定性映射：
  //   命中 -> 掐断云端回复 + 以 CommandSource::Voice 派发 + 屏上提示；
  //   未命中 -> 仅给出支持短语提示（学习页不做自由对话）。
  static void OnVoicePhrase(const char* text);

  // L4：**线程安全**的语音文本投递口。
  //
  // display 层（LVAdapterDisplay::SetChatMessage）是在**协议任务**上下文被调用的，
  // 而 LVGL 并非线程安全 —— 在那里直接调用 OnVoicePhrase()（内部有
  // lv_label_set_text / lv_timer_del）会与 LVGL 任务里的 RefreshUi() 并发改动
  // 同一批对象，实测导致 lv_inv_area 失效区链表损坏、LVGL 重绘死循环，
  // 进而饿死 IDLE1 触发 task_wdt 全系统卡死。
  //
  // 因此协议任务**只入队**（仅 std::mutex + deque，不触碰 LVGL），
  // 由学习屏自己的 LVGL 定时器在 LVGL 任务上下文取出再执行 OnVoicePhrase()。
  static void QueueVoicePhrase(const char* text);
};
