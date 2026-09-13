// claw4 P18/P18-L1 Learning App screen (device build tree).
// WB-LEARNING-V4-L1 — the screen now renders the REAL domain state driven by
// the host-verified funnel (LearningApp over NVS storage + esp_timer clock):
//   * UI keeps NO business state: every render is a projection of
//     LearningRuntime::app().state() refreshed on a 1 s LVGL timer;
//   * the only mutation path is dispatcher.dispatch(CommandSource::Touch, ..)
//     (P14 funnel; Complete from a Touch source is emitted directly);
//   * first boot seeds the demo today snapshot (see LearningRuntime::Init);
//   * session focus seconds are projected live from the monotonic clock while
//     Running (read-only; the domain accumulates on state transitions).
// L0 mock (s_ui.running/done) is removed.
#include "learning_screen/learning_screen.h"

#include <cstdio>
#include <deque>
#include <mutex>
#include <string>

#include "esp_log.h"
#include "home_screen/home_screen.h"

#include "interaction/command.h"
#include "interaction/stt_mapper.h"
#include "learning_domain/domain_state.h"
#include "metalio_claw4/device/app/learning_runtime.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "LearningScreen";
constexpr int kPanelW = 720;
constexpr int kPanelH = 720;
constexpr uint32_t kRefreshMs = 1000;

using claw4::domain::DomainState;
using claw4::domain::SessionStatus;
using claw4::domain::StudySession;
using claw4::domain::Task;
using claw4::domain::TaskId;
using claw4::domain::TaskStatus;
using claw4::interaction::CommandKind;
using claw4::interaction::CommandPayload;
using claw4::interaction::CommandSource;
using claw4::metalio::LearningRuntime;

struct Ui {
  lv_obj_t* row_a = nullptr;      // first task line
  lv_obj_t* row_b = nullptr;      // second task line
  lv_obj_t* row_more = nullptr;   // "… and N more" / empty hint
  lv_obj_t* session = nullptr;    // big session/status text (multi-line)
  lv_obj_t* diagnostics = nullptr;  // pending/ACK/boot gate, no hardware claims
  lv_obj_t* primary = nullptr;    // 开始/暂停/继续 button
  lv_obj_t* primary_lbl = nullptr;
  lv_obj_t* secondary = nullptr;  // 完成 button
  lv_obj_t* secondary_lbl = nullptr;
  lv_obj_t* voice = nullptr;      // L4: 语音命令反馈行
  lv_timer_t* timer = nullptr;
};
Ui s_ui;

// L4 (P25)：学习屏是否在前台 —— display 层据此路由语音识别文本。
bool s_active = false;

// L4：按需开麦。学习页**不常开**语音会话 —— AFE 持续 feed 会与后台同步争抢
// CPU（实测触发 task_wdt 全系统卡死）。改为「语音」按钮开启一个时间窗。
// 窗口需覆盖「唤醒 + 说命令 + ASR 往返」，且每次收到语音文本都会续期。
constexpr uint32_t kVoiceWindowMs = 15000;
bool s_voice_open = false;
// ⚠️ 窗口超时用【时间戳】而不是一次性 lv_timer：
// `lv_timer_set_repeat_count(t, 1)` 会让 LVGL 在到期后**自动删除**该 timer，
// 之后再手动 `lv_timer_del()` 同一指针就是 use-after-free → 堆破坏
// （实测：`assert failed: xQueueSemaphoreTake queue.c:1713 (pxQueue->uxItemSize == 0)`
//  → SW_CPU_RESET 重启）。改为在既有的 50 ms poll timer 里比较 lv_tick 时间戳，
// **全程不新增、不删除任何 timer**，删除权只归 s_voice_poll_timer 一家。
uint32_t s_voice_deadline = 0;  // lv_tick_get() 到期时刻；0 = 窗口未开启
void SetVoiceHint(const char* text);  // fwd（定义在下方语音段落）
void CloseVoiceWindow(const char* hint);  // fwd（定义在下方语音段落）

// L4：跨任务语音文本队列（详见头文件 QueueVoicePhrase 的说明）。
// 协议任务只 push（仅碰 std::mutex + deque），LVGL 定时器 pop 后在
// LVGL 任务上下文执行 —— 避免跨任务并发操作 LVGL。
constexpr size_t kVoiceQueueMax = 8;
std::mutex s_voice_q_mtx;
std::deque<std::string> s_voice_q;
lv_timer_t* s_voice_poll_timer = nullptr;
constexpr uint32_t kVoicePollMs = 50;

LearningRuntime& Rt() { return LearningRuntime::Instance(); }

void RefreshUi();  // fwd (defined below; used by the self-test step chain)

// --- one-shot funnel self-test (device debug aid) -------------------------
// Boots into the Learning screen once: automatically walks Start -> Pause ->
// Resume -> Complete against the REAL domain/NVS pipeline, logs each result,
// then resets the learning namespace to a clean seeded demo state. Running it
// requires no touch input (the user only needs to open the screen once).
int s_selftest_step = 0;
bool s_selftest_ok[4] = {false, false, false, false};
lv_timer_t* s_selftest_timer = nullptr;

void RunSelfTestStep(lv_timer_t*) {
  auto& rt = Rt();
  std::lock_guard<std::recursive_mutex> lock(rt.stateMutex());
  auto& app = rt.app();
  const int step = s_selftest_step++;
  if (step < 4) {
    CommandPayload p;
    p.task_id = claw4::domain::TaskId{"demo-math-001"};
    switch (step) {
      case 0: p.kind = CommandKind::StartTask; break;
      case 1: p.kind = CommandKind::PauseTask; break;
      case 2: p.kind = CommandKind::ResumeTask; break;
      default: p.kind = CommandKind::CompleteTask; break;
    }
    const auto r = app.dispatcher().dispatch(CommandSource::Touch, p);
    const bool ok = r.intent_result == claw4::domain::IntentResult::Accepted;
    s_selftest_ok[step] = ok;
    ESP_LOGI(TAG, "SELFTEST step%d kind=%d -> status=%d intent=%d ok=%d",
             step, static_cast<int>(p.kind), static_cast<int>(r.status),
             static_cast<int>(r.intent_result), ok ? 1 : 0);
    return;
  }
  // Cleanup: stop the chain, report, reset to a clean demo state.
  if (s_selftest_timer != nullptr) {
    lv_timer_del(s_selftest_timer);
    s_selftest_timer = nullptr;
  }
  const bool all = s_selftest_ok[0] && s_selftest_ok[1] && s_selftest_ok[2] &&
                   s_selftest_ok[3];
  ESP_LOGI(TAG, "SELFTEST %s (start=%d pause=%d resume=%d complete=%d)",
           all ? "PASS" : "FAIL", s_selftest_ok[0] ? 1 : 0,
           s_selftest_ok[1] ? 1 : 0, s_selftest_ok[2] ? 1 : 0,
           s_selftest_ok[3] ? 1 : 0);
  // ResetToSeed erases the complete "learning" namespace, including the
  // self-test marker. Persist the marker only after a successful reset so the
  // one-shot chain does not run again on the next screen entry/reboot.
  const bool reset_ok = rt.ResetToSeed();
  if (reset_ok) rt.MarkSelfTestDone();
  RefreshUi();
  ESP_LOGI(TAG, "SELFTEST cleanup done -> state re-seeded=%d",
           reset_ok ? 1 : 0);
}

// --- pure view helpers ---------------------------------------------------
const char* TaskStatusWord(TaskStatus s) {
  switch (s) {
    case TaskStatus::Pending: return "已排期";
    case TaskStatus::Ready: return "待开始";
    case TaskStatus::InProgress: return "进行中";
    case TaskStatus::Paused: return "已暂停";
    case TaskStatus::Completed: return "已完成";
    case TaskStatus::Skipped: return "已跳过";
  }
  return "?";
}

std::string Mmss(int64_t seconds) {
  if (seconds < 0) seconds = 0;
  const long long m = seconds / 60;
  const long long s = seconds % 60;
  std::string out;
  if (m < 10) out += '0';
  out += std::to_string(m);
  out += ':';
  if (s < 10) out += '0';
  out += std::to_string(s);
  return out;
}

const Task* FindTask(const DomainState& st, const TaskId& id) {
  for (const auto& t : st.tasks) {
    if (t.task_id == id) return &t;
  }
  return nullptr;
}

const Task* FirstReady(const DomainState& st) {
  for (const auto& t : st.tasks) {
    if (t.status == TaskStatus::Ready) return &t;
  }
  return nullptr;
}

bool AllFinished(const DomainState& st) {
  if (st.tasks.empty()) return false;
  for (const auto& t : st.tasks) {
    if (t.status != TaskStatus::Completed && t.status != TaskStatus::Skipped) {
      return false;
    }
  }
  return true;
}

int64_t LiveActualSeconds(const StudySession& s) {
  if (s.status == SessionStatus::Running) {
    // Project running seconds from the monotonic clock (read-only; the domain
    // accumulates actual_seconds on state transitions).
    const int64_t ms = Rt().clock().monotonicMs();
    const int64_t sec = s.actual_seconds + (ms - s.segment_start_monotonic_ms) / 1000;
    return sec >= 0 ? sec : s.actual_seconds;
  }
  return s.actual_seconds;
}

void DispatchTouch(CommandKind kind, const TaskId& task_id) {
  auto& app = Rt().app();
  CommandPayload p;
  p.kind = kind;
  p.task_id = task_id;
  const auto r = app.dispatcher().dispatch(CommandSource::Touch, p);
  ESP_LOGI(TAG, "touch kind=%d task=%s -> status=%d intent=%d",
           static_cast<int>(kind), task_id.value.c_str(),
           static_cast<int>(r.status), static_cast<int>(r.intent_result));
  if (r.intent_result == claw4::domain::IntentResult::PersistFailed) {
    const DomainState& st = app.state();
    const std::string last_acked =
        std::to_string(app.coordinator().lastAcked());
    ESP_LOGW(TAG,
             "persist-fail diag: tasks=%d active=%d pending=%d lastAcked=%s "
             "nextPending=%d",
             (int)st.tasks.size(), st.active_session.has_value() ? 1 : 0,
             app.pendingCount(),
             last_acked.c_str(),
             app.coordinator().pendingCount());
  }
}

// --- refresh (pure projection of committed domain state) ------------------
void RefreshUi() {
  std::lock_guard<std::recursive_mutex> lock(Rt().stateMutex());
  if (!Rt().bootReady()) {
    lv_label_set_text(s_ui.row_a, "学习状态恢复失败");
    lv_label_set_text(s_ui.row_b, "请返回后重试");
    lv_label_set_text(s_ui.row_more, "启动恢复门禁未通过，未接受任何操作");
    lv_label_set_text(s_ui.session, "暂不可用");
    lv_label_set_text(s_ui.primary_lbl, "—");
    lv_label_set_text(s_ui.secondary_lbl, "—");
    lv_obj_remove_flag(s_ui.primary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_ui.secondary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_ui.primary, LV_OPA_40, 0);
    lv_obj_set_style_opa(s_ui.secondary, LV_OPA_40, 0);
    lv_label_set_text(s_ui.diagnostics,
                      "诊断 · boot recovery failed · 未执行网络/业务操作");
    return;
  }
  auto& app = Rt().app();
  const DomainState& st = app.state();
  const bool has_active = st.active_session.has_value();
  const Task* active_task =
      has_active ? FindTask(st, st.active_session->task_id) : nullptr;
  const Task* first_ready = FirstReady(st);

  // Task rows.
  std::string row_a = "今日暂无任务（等待同步）";
  std::string row_b;
  std::string more;
  if (!st.tasks.empty()) {
    row_a = st.tasks[0].title + "  ·  " + TaskStatusWord(st.tasks[0].status);
    if (st.tasks.size() >= 2) {
      row_b = st.tasks[1].title + "  ·  " + TaskStatusWord(st.tasks[1].status);
    }
    if (st.tasks.size() > 2) {
      more = "… 共 " + std::to_string(st.tasks.size()) + " 项任务";
    }
  }
  lv_label_set_text(s_ui.row_a, row_a.c_str());
  if (s_ui.row_b) lv_label_set_text(s_ui.row_b, row_b.c_str());
  if (s_ui.row_more) lv_label_set_text(s_ui.row_more, more.c_str());

  // Session / status area.
  std::string sess;
  if (!has_active) {
    if (st.tasks.empty()) {
      sess = "未开始";
    } else if (AllFinished(st)) {
      sess = "今日任务已全部完成";
    } else {
      int todo = 0;
      for (const auto& t : st.tasks) {
        if (t.status == TaskStatus::Ready) ++todo;
      }
      sess = "未开始 · 待办 " + std::to_string(todo) + " 项";
    }
  } else {
    const StudySession& s = *st.active_session;
    const std::string title =
        active_task ? active_task->title : std::string("任务");
    const int64_t live = LiveActualSeconds(s);
    const int64_t planned = s.planned_minutes;
    if (s.status == SessionStatus::Running) {
      sess = "专注中 · " + title + "\n已专注 " + Mmss(live) + " / " +
             std::to_string(planned) + " 分钟";
    } else if (s.status == SessionStatus::Paused) {
      sess = "已暂停 · " + title + "\n已专注 " + Mmss(live) + " / " +
             std::to_string(planned) + " 分钟";
    } else {
      sess = "任务进行中 · " + title;
    }
  }
  lv_label_set_text(s_ui.session, sess.c_str());

  // Primary action (开始今日任务 / 暂停 / 继续).
  const char* primary_text = "开始今日任务";
  bool primary_en = true;
  if (has_active) {
    const SessionStatus ss = st.active_session->status;
    if (ss == SessionStatus::Running) {
      primary_text = "暂停";
    } else if (ss == SessionStatus::Paused) {
      primary_text = "继续";
    } else {
      primary_text = "—";
      primary_en = false;
    }
  } else if (first_ready == nullptr) {
    if (AllFinished(st)) {
      primary_text = "重新生成演示任务";
    } else {
      primary_en = false;  // nothing runnable (empty snapshot)
    }
  }
  lv_label_set_text(s_ui.primary_lbl, primary_text);
  if (primary_en) {
    lv_obj_add_flag(s_ui.primary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_ui.primary, LV_OPA_COVER, 0);
  } else {
    lv_obj_remove_flag(s_ui.primary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_ui.primary, LV_OPA_40, 0);
  }

  // Secondary action (完成 — Touch emits Complete directly, P14 gate).
  const bool sec_en = has_active;
  lv_label_set_text(s_ui.secondary_lbl, sec_en ? "完成" : "—");
  if (sec_en) {
    lv_obj_add_flag(s_ui.secondary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_ui.secondary, LV_OPA_COVER, 0);
  } else {
    lv_obj_remove_flag(s_ui.secondary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_ui.secondary, LV_OPA_40, 0);
  }

  const auto backend = Rt().BackendDiagnostics();
  std::string diagnostics =
      "诊断 · build app-first · pending " +
      std::to_string(app.pendingCount()) + " · ACK " +
      std::to_string(app.coordinator().lastAcked());
  if (!backend.configured) {
    diagnostics += " · backend 未配置";
  } else {
    diagnostics += backend.network_online ? " · 网络在线" : " · 网络离线";
    diagnostics += backend.authenticated ? " · auth OK" : " · auth 未通过";
    diagnostics += " · err=" +
                   std::to_string(static_cast<int>(backend.last_error));
    diagnostics += " · op=" + backend.last_operation;
  }
  lv_label_set_text(s_ui.diagnostics, diagnostics.c_str());
}

// --- callbacks -------------------------------------------------------------
void OnPrimary(lv_event_t*) {
  std::lock_guard<std::recursive_mutex> lock(Rt().stateMutex());
  auto& app = Rt().app();
  const DomainState& st = app.state();
  if (st.active_session.has_value()) {
    const TaskId tid = st.active_session->task_id;
    if (st.active_session->status == SessionStatus::Running) {
      DispatchTouch(CommandKind::PauseTask, tid);
    } else if (st.active_session->status == SessionStatus::Paused) {
      DispatchTouch(CommandKind::ResumeTask, tid);
    }
  } else {
    const Task* first = FirstReady(st);
    if (first != nullptr) {
      DispatchTouch(CommandKind::StartTask, first->task_id);
    } else if (AllFinished(st)) {
      const bool reset_ok = Rt().ResetToSeed();
      ESP_LOGI(TAG, "demo reset requested from completed state -> ok=%d",
               reset_ok ? 1 : 0);
    }
  }
  RefreshUi();
}

void OnSecondary(lv_event_t*) {
  std::lock_guard<std::recursive_mutex> lock(Rt().stateMutex());
  auto& app = Rt().app();
  const DomainState& st = app.state();
  if (st.active_session.has_value()) {
    DispatchTouch(CommandKind::CompleteTask, st.active_session->task_id);
  }
  RefreshUi();
}

void OnRefreshTick(lv_timer_t*) { RefreshUi(); }

// --- L4 voice (P25 Voice STT) ----------------------------------------------

void SetVoiceHint(const char* text) {
  if (s_ui.voice == nullptr) return;
  lv_label_set_text(s_ui.voice, text != nullptr ? text : "");
}

const char* KindLabel(CommandKind kind) {
  switch (kind) {
    case CommandKind::StartTask:          return "开始";
    case CommandKind::PauseTask:          return "暂停";
    case CommandKind::ResumeTask:         return "继续";
    case CommandKind::CompleteTask:       return "完成";
    case CommandKind::SkipTask:           return "跳过";
    case CommandKind::QueryTodayTasks:    return "今天任务";
    case CommandKind::QueryCurrentTask:   return "当前任务";
    case CommandKind::QueryRemainingTime: return "剩余时间";
    case CommandKind::QueryTodayProgress: return "今日进度";
    default:                              return "未知";
  }
}

// 查询类命令的答复文本（纯读，不改状态；dispatcher 明确不转发 query）。
std::string VoiceQueryText(CommandKind kind, const DomainState& st) {
  int total = 0;
  int done = 0;
  for (const auto& t : st.tasks) {
    ++total;
    if (t.status == TaskStatus::Completed || t.status == TaskStatus::Skipped) ++done;
  }

  switch (kind) {
    case CommandKind::QueryTodayTasks:
      return "今日任务共 " + std::to_string(total) + " 个，未完成 " +
             std::to_string(total - done) + " 个";
    case CommandKind::QueryTodayProgress:
      return "今日进度 " + std::to_string(done) + "/" + std::to_string(total);
    case CommandKind::QueryCurrentTask: {
      if (!st.active_session.has_value()) return "当前没有进行中的任务";
      const Task* t = FindTask(st, st.active_session->task_id);
      return std::string("当前任务：") + (t != nullptr ? t->title : "未知任务");
    }
    case CommandKind::QueryRemainingTime: {
      if (!st.active_session.has_value()) return "当前没有进行中的任务";
      const StudySession& s = *st.active_session;
      const Task* t = FindTask(st, s.task_id);
      const int planned = (t != nullptr && t->estimated_minutes > 0)
                              ? t->estimated_minutes
                              : s.planned_minutes;
      long long remain = static_cast<long long>(planned) * 60 - s.actual_seconds;
      if (remain < 0) remain = 0;
      return "还剩约 " + std::to_string(remain / 60) + " 分钟";
    }
    default:
      return "已识别";
  }
}

void DispatchVoice(CommandKind kind, const TaskId& task_id) {
  auto& app = Rt().app();
  CommandPayload p;
  p.kind = kind;
  p.task_id = task_id;
  const auto r = app.dispatcher().dispatch(CommandSource::Voice, p);
  ESP_LOGI(TAG, "voice kind=%d task=%s -> status=%d intent=%d",
           static_cast<int>(kind), task_id.value.c_str(),
           static_cast<int>(r.status), static_cast<int>(r.intent_result));
}

// --- L4 按需开麦（语音时间窗） ---------------------------------------------

void CloseVoiceWindow(const char* hint) {
  // 只清时间戳 —— 不碰任何 timer（timer 生命周期见文件头注释）。
  s_voice_deadline = 0;
  if (s_voice_open) {
    s_voice_open = false;
    Rt().voiceSession().endSession();
    ESP_LOGI(TAG, "voice window closed");
  }
  if (hint != nullptr) SetVoiceHint(hint);
}

void ArmVoiceWindow() {
  auto& rt = Rt();
  if (!rt.bootReady()) return;

  if (!s_voice_open) {
    if (!rt.voiceSession().beginSession()) {
      SetVoiceHint("语音会话获取失败");
      return;
    }
    s_voice_open = true;
    ESP_LOGI(TAG, "voice window armed (%u ms)", (unsigned)kVoiceWindowMs);
  }
  // 续期：仅刷新到期时刻（收到语音文本时也会走到这里）。
  s_voice_deadline = lv_tick_get() + kVoiceWindowMs;
  SetVoiceHint("语音已开启，请说「嗨钛灵」唤醒后再下达命令");
}

void OnVoiceButton(lv_event_t*) {
  auto& rt = Rt();
  std::lock_guard<std::recursive_mutex> lock(rt.stateMutex());
  ArmVoiceWindow();
}

void GoHome() {
  lv_obj_t* old_scr = lv_screen_active();
  lv_obj_t* home = HomeScreen::Create();
  lv_screen_load(home);
  if (old_scr != nullptr && old_scr != home) {
    lv_obj_delete_async(old_scr);
  }
}

lv_obj_t* MakeButton(lv_obj_t* parent, int x, int y, int w, int h,
                     const char* text, lv_event_cb_t handler) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x1F2733), LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x34415A),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn, 16, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, lv_color_hex(0x3A4657), LV_PART_MAIN);
  lv_obj_add_event_cb(btn, handler, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_text_font(label, &font_puhui_30_4, 0);
  lv_obj_center(label);
  return btn;
}

}  // namespace

void LearningScreen::LifecycleCallback(screen_lifecycle_event_t event) {
  if (event == SCREEN_LIFECYCLE_LOAD) {
    s_active = true;
    // L4: 不自动开麦 —— 点「语音」按钮开启 8 秒时间窗。常开语音会让 AFE 持续
    // feed 并与后台同步争抢 CPU（实测触发 task_wdt 全系统卡死）。
    SetVoiceHint("点右上角「语音」开麦，说「嗨钛灵」唤醒后再下命令");
    ESP_LOGI(TAG, "load: learning_screen (real state, voice on demand)");
  } else {
    s_active = false;
    // L4: 释放语音会话（若时间窗仍开着）并清掉定时器。
    CloseVoiceWindow(nullptr);
    ESP_LOGI(TAG, "unload: learning_screen (voice session released)");
    if (s_ui.timer != nullptr) {
      lv_timer_del(s_ui.timer);
      s_ui.timer = nullptr;
    }
    if (s_selftest_timer != nullptr) {
      lv_timer_del(s_selftest_timer);
      s_selftest_timer = nullptr;
    }
    if (s_voice_poll_timer != nullptr) {
      lv_timer_del(s_voice_poll_timer);
      s_voice_poll_timer = nullptr;
    }
    {
      std::lock_guard<std::mutex> lock(s_voice_q_mtx);
      s_voice_q.clear();
    }
  }
}

bool LearningScreen::IsActive() { return s_active; }

// ---- L4 跨任务投递：协议任务 -> 队列 -> LVGL 定时器 -------------------------
// 协议任务（LVAdapterDisplay::SetChatMessage 的调用者）绝不能直接调 LVGL：
// LVGL 非线程安全，与 LVGL 任务里的 RefreshUi() 并发会破坏失效区链表，
// 实测导致 lv_inv_area 重绘死循环 -> IDLE1 饿死 -> task_wdt 全系统卡死。
void LearningScreen::QueueVoicePhrase(const char* text) {
  if (text == nullptr || text[0] == '\0') return;
  std::lock_guard<std::mutex> lock(s_voice_q_mtx);
  if (s_voice_q.size() >= kVoiceQueueMax) {
    s_voice_q.pop_front();  // 极端情况丢最旧的，绝不无界增长
  }
  s_voice_q.emplace_back(text);
}

// LVGL 定时器回调 —— 运行在 LVGL 任务上下文，此处操作 LVGL 是安全的。
void OnVoicePollTick(lv_timer_t*) {
  for (;;) {
    std::string phrase;
    {
      std::lock_guard<std::mutex> lock(s_voice_q_mtx);
      if (s_voice_q.empty()) break;
      phrase = std::move(s_voice_q.front());
      s_voice_q.pop_front();
    }
    LearningScreen::OnVoicePhrase(phrase.c_str());
  }

  // 时间窗超时检查 —— 复用本 timer，不新增/删除任何 timer（见文件头注释）。
  if (s_voice_open && s_voice_deadline != 0 &&
      static_cast<int32_t>(lv_tick_get() - s_voice_deadline) >= 0) {
    CloseVoiceWindow("语音已关闭（15 秒未识别）");
  }
}

void LearningScreen::OnVoicePhrase(const char* text) {
  if (text == nullptr || text[0] == '\0') return;

  auto& rt = Rt();
  std::lock_guard<std::recursive_mutex> lock(rt.stateMutex());
  if (!rt.bootReady()) return;

  // 只要有语音文本回来就续期时间窗 —— 唤醒词回声到达时窗口往往已过大半，
  // 不续期会导致用户还没说出命令就被关麦。
  ArmVoiceWindow();

  const claw4::interaction::SttMapping m =
      claw4::interaction::mapVoicePhrase(text);

  if (!m.recognized) {
    // 未识别为学习命令：**不掐断、不关麦**。
    // 上游会把唤醒词音频也送入云端 ASR（返回形如 "Hi 钛灵"），若在此掐断或
    // 关闭时间窗，会杀死刚建立的对话流程，导致后续真正的命令收不到。
    // 时间窗仍由 8 秒定时器兜底关闭。
    SetVoiceHint("未识别，请继续说：开始/暂停/继续/完成/跳过/还有多久/当前任务/今天任务");
    ESP_LOGI(TAG, "voice phrase not a learning command (session kept): %s", text);
    return;
  }

  const DomainState& st = rt.app().state();

  // 命中学习命令：先掐断云端 LLM/TTS，避免孩子听到无关闲聊
  //（上游为 ASR→LLM 流水线，可能已有残余输出）。
  rt.voiceSession().abortCloudReply();

  // 查询类：只读答复，不进入派发（dispatcher 不转发 query kind）。
  if (m.kind == CommandKind::QueryTodayTasks ||
      m.kind == CommandKind::QueryCurrentTask ||
      m.kind == CommandKind::QueryRemainingTime ||
      m.kind == CommandKind::QueryTodayProgress) {
    const std::string ans = VoiceQueryText(m.kind, st);
    CloseVoiceWindow(ans.c_str());
    ESP_LOGI(TAG, "voice query (%s) -> %s", KindLabel(m.kind), ans.c_str());
    return;
  }

  // 变更类：目标 = 当前活动会话的 task，否则第一个 Ready 任务。
  TaskId target{};
  if (st.active_session.has_value()) {
    target = st.active_session->task_id;
  } else {
    const Task* first = FirstReady(st);
    if (first != nullptr) target = first->task_id;
  }

  DispatchVoice(m.kind, target);
  RefreshUi();

  std::string hint = std::string("语音已识别：") + KindLabel(m.kind);
  if (rt.app().dispatcher().hasPendingComplete()) {
    // 「完成」走受控路径：AI 不能直接完成任务，需物理确认。
    hint += "（请在屏幕上确认）";
  }
  // 命令已收到，关麦释放 CPU（再次使用需重新点「语音」）。
  CloseVoiceWindow(hint.c_str());
}

lv_obj_t* LearningScreen::Create() {
  LearningRuntime& rt = Rt();
  rt.Init();  // construct app over NVS+clock; seed demo on first boot

  lv_obj_t* scr = lv_obj_create(nullptr);
  screen_strip_obj_chrome(scr);
  lv_obj_set_size(scr, kPanelW, kPanelH);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E1116), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Header (back at top-left, title centred).
  lv_obj_t* header = lv_obj_create(scr);
  screen_strip_obj_chrome(header);
  lv_obj_set_size(header, kPanelW, 90);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "学习");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &font_puhui_30_4, 0);
  lv_obj_center(title);
  MakeButton(header, 16, 21, 110, 48, "返回", [](lv_event_t*) { GoHome(); });
  // L4: 按需开麦按钮（避免常开语音会话让 AFE 持续 feed 抢占 CPU）。
  MakeButton(header, 586, 21, 118, 48, "语音", OnVoiceButton);

  // Today list section.
  lv_obj_t* section = lv_label_create(scr);
  lv_label_set_text(section, "今日任务");
  lv_obj_set_style_text_color(section, lv_color_hex(0x8A93A6), 0);
  lv_obj_set_style_text_font(section, &font_puhui_20_4, 0);
  lv_obj_set_pos(section, 48, 106);

  s_ui.row_a = lv_label_create(scr);
  lv_obj_set_style_text_color(s_ui.row_a, lv_color_white(), 0);
  lv_obj_set_style_text_font(s_ui.row_a, &font_puhui_30_4, 0);
  lv_obj_set_pos(s_ui.row_a, 48, 142);

  s_ui.row_b = lv_label_create(scr);
  lv_obj_set_style_text_color(s_ui.row_b, lv_color_white(), 0);
  lv_obj_set_style_text_font(s_ui.row_b, &font_puhui_30_4, 0);
  lv_obj_set_pos(s_ui.row_b, 48, 198);

  s_ui.row_more = lv_label_create(scr);
  lv_obj_set_style_text_color(s_ui.row_more, lv_color_hex(0x8A93A6), 0);
  lv_obj_set_style_text_font(s_ui.row_more, &font_puhui_20_4, 0);
  lv_obj_set_pos(s_ui.row_more, 48, 254);

  // Session / status panel.
  lv_obj_t* panel = lv_obj_create(scr);
  screen_strip_obj_chrome(panel);
  lv_obj_set_size(panel, 640, 150);
  lv_obj_set_pos(panel, 40, 288);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x151B26), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 16, LV_PART_MAIN);
  s_ui.session = lv_label_create(panel);
  lv_obj_set_style_text_color(s_ui.session, lv_color_white(), 0);
  lv_obj_set_style_text_font(s_ui.session, &font_puhui_30_4, 0);
  lv_obj_set_style_text_align(s_ui.session, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(s_ui.session);
  s_ui.diagnostics = lv_label_create(panel);
  lv_obj_set_style_text_color(s_ui.diagnostics, lv_color_hex(0x8A93A6), 0);
  lv_obj_set_style_text_font(s_ui.diagnostics, &font_puhui_20_4, 0);
  lv_obj_set_width(s_ui.diagnostics, 600);
  lv_label_set_long_mode(s_ui.diagnostics, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(s_ui.diagnostics, 20, 116);

  // Primary / secondary actions.
  s_ui.primary = MakeButton(scr, 140, 470, 440, 88, "—", OnPrimary);
  s_ui.primary_lbl = lv_obj_get_child(s_ui.primary, 0);
  s_ui.secondary = MakeButton(scr, 140, 574, 440, 76, "—", OnSecondary);
  s_ui.secondary_lbl = lv_obj_get_child(s_ui.secondary, 0);

  // L4: 语音命令反馈行（secondary 按钮下方，720 屏高内）。
  s_ui.voice = lv_label_create(scr);
  lv_label_set_text(s_ui.voice, "");
  lv_obj_set_style_text_color(s_ui.voice, lv_color_hex(0x5DCAA5), 0);
  lv_obj_set_style_text_font(s_ui.voice, &font_puhui_20_4, 0);
  lv_obj_set_width(s_ui.voice, 640);
  lv_label_set_long_mode(s_ui.voice, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(s_ui.voice, 48, 662);

  RefreshUi();
  s_ui.timer = lv_timer_create(OnRefreshTick, kRefreshMs, nullptr);
  // L4：语音文本队列轮询（LVGL 上下文取出处理，见 QueueVoicePhrase）。
  if (s_voice_poll_timer == nullptr) {
    s_voice_poll_timer = lv_timer_create(OnVoicePollTick, kVoicePollMs, nullptr);
  }
  if (rt.bootReady() && rt.SelfTestPending() && s_selftest_timer == nullptr) {
    s_selftest_step = 0;
    s_selftest_ok[0] = s_selftest_ok[1] = s_selftest_ok[2] = s_selftest_ok[3] = false;
    s_selftest_timer = lv_timer_create(RunSelfTestStep, 600, nullptr);
    ESP_LOGI(TAG, "SELFTEST armed (auto 4-step funnel chain)");
  }
  return scr;
}
