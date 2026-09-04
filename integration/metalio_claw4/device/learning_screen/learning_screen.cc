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
#include <string>

#include "esp_log.h"
#include "home_screen/home_screen.h"

#include "interaction/command.h"
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
  lv_obj_t* primary = nullptr;    // 开始/暂停/继续 button
  lv_obj_t* primary_lbl = nullptr;
  lv_obj_t* secondary = nullptr;  // 完成 button
  lv_obj_t* secondary_lbl = nullptr;
  lv_timer_t* timer = nullptr;
};
Ui s_ui;

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
  rt.MarkSelfTestDone();
  rt.ResetToSeed();
  RefreshUi();
  ESP_LOGI(TAG, "SELFTEST cleanup done -> state re-seeded to demo");
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
  if (s.status == SessionStatus::Running && s.segment_start_monotonic_ms > 0) {
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
    primary_en = false;  // nothing runnable (all finished or empty)
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
}

// --- callbacks -------------------------------------------------------------
void OnPrimary(lv_event_t*) {
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
    if (first != nullptr) DispatchTouch(CommandKind::StartTask, first->task_id);
  }
  RefreshUi();
}

void OnSecondary(lv_event_t*) {
  auto& app = Rt().app();
  const DomainState& st = app.state();
  if (st.active_session.has_value()) {
    DispatchTouch(CommandKind::CompleteTask, st.active_session->task_id);
  }
  RefreshUi();
}

void OnRefreshTick(lv_timer_t*) { RefreshUi(); }

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
    ESP_LOGI(TAG, "load: learning_screen (real state)");
  } else {
    ESP_LOGI(TAG, "unload: learning_screen");
    if (s_ui.timer != nullptr) {
      lv_timer_del(s_ui.timer);
      s_ui.timer = nullptr;
    }
    if (s_selftest_timer != nullptr) {
      lv_timer_del(s_selftest_timer);
      s_selftest_timer = nullptr;
    }
  }
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

  // Primary / secondary actions.
  s_ui.primary = MakeButton(scr, 140, 470, 440, 88, "—", OnPrimary);
  s_ui.primary_lbl = lv_obj_get_child(s_ui.primary, 0);
  s_ui.secondary = MakeButton(scr, 140, 574, 440, 76, "—", OnSecondary);
  s_ui.secondary_lbl = lv_obj_get_child(s_ui.secondary, 0);

  RefreshUi();
  s_ui.timer = lv_timer_create(OnRefreshTick, kRefreshMs, nullptr);
  if (rt.SelfTestPending() && s_selftest_timer == nullptr) {
    s_selftest_step = 0;
    s_selftest_ok[0] = s_selftest_ok[1] = s_selftest_ok[2] = s_selftest_ok[3] = false;
    s_selftest_timer = lv_timer_create(RunSelfTestStep, 600, nullptr);
    ESP_LOGI(TAG, "SELFTEST armed (auto 4-step funnel chain)");
  }
  return scr;
}
