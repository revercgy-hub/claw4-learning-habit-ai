#include "learning_preview.h"

#ifdef CLAW4_LEARNING_PREVIEW
#include "application.h"
#include "application/learning_voice_commands.h"
#include "assets/lang_config.h"
#include "metalio_claw4/device/ports/nvs_outbox_storage.h"
#include "timer_preview.h"

#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string_view>

LV_FONT_DECLARE(font_noto_sans_basic_30_4);

namespace {
using namespace claw4;
using Kind = application::LearningVoiceKind;
using Status = application::LearningVoiceStatus;
constexpr char kTag[] = "Claw4Learning";
constexpr uint32_t kInk = 0x233D3B, kMuted = 0x4B6461, kMint = 0xBDE9D1;
constexpr uint32_t kLilac = 0xE9DFF6, kPaper = 0xF7F3E9, kWhite = 0xFFFDF7;
const application::LearningVoiceBinding binding{{"v6-preview"}, {"demo"}, 1};

class Clock final : public ports::ClockPort {
 public:
  int64_t epochSeconds() override { return 0; }
  int64_t monotonicMs() override { return esp_timer_get_time() / 1000; }
  bool isTimeSynced() override { return false; }
};
class NoReminderStorage final : public reminder::ReminderStore {
 public:
  bool load(std::vector<reminder::ReminderRecord>& out) override { out.clear(); return true; }
  bool save(const std::vector<reminder::ReminderRecord>&) override { return false; }
};

std::string Id(const char* kind) {
  char result[48];
  std::snprintf(result, sizeof(result), "%s-%08x%08x%08x", kind,
                unsigned(esp_random()), unsigned(esp_random()), unsigned(esp_random()));
  return result;
}

struct Runtime {
  Clock clock;
  time::TimeAuthority calendar{clock, "local-preview"};
  NoReminderStorage reminder_store;
  reminder::ReminderCore reminders{{}, reminder_store, calendar};
  metalio::NvsOutboxStorage storage{"claw4_v6demo"};
  domain::DomainReducer reducer;
  application::AppCoordinator coordinator{storage, reducer};
  application::LearningVoiceCommands commands{coordinator, reminders};
  domain::TaskId selection{"math"};
  domain::SessionId cue_session;
  bool ready = false, confirming = false, cue_played = false;
  const char* feedback = "";

  Runtime() {
    sync::OutboxState previous;
    const auto presence = storage.loadWithPresence(previous);
    ready = presence != metalio::NvsOutboxStorage::StateReadStatus::Error &&
            coordinator.prepareAfterBoot(clock.monotonicMs());
    if (ready && presence == metalio::NvsOutboxStorage::StateReadStatus::Missing) {
      domain::Task math;
      math.task_id = {"math"};
      math.child_id = binding.child_id;
      math.title = "Math warm-up";
      math.status = domain::TaskStatus::Ready;
      math.estimated_minutes = 1;
      math.version = 1;
      auto reading = math;
      reading.task_id = {"reading"};
      reading.title = "Reading break";
      ready = coordinator.applyTodaySnapshot({math, reading});
    }
    ESP_LOGI(kTag, "LOCAL_PREVIEW_READY=%d", ready ? 1 : 0);
  }
  domain::ReducerContext context(int64_t now) {
    return {binding.device_id, binding.child_id, now / 1000, clock.monotonicMs(),
            [] { return domain::EventId{Id("event")}; },
            [] { return domain::SessionId{Id("study")}; }};
  }
};

enum class Tap { Open, Close, Math, Reading, StartPause, Finish, Confirm, Cancel };
std::unique_ptr<Runtime> runtime;
std::atomic<bool> refresh_pending{false};
lv_obj_t *panel, *counter, *progress, *state_label, *feedback_label;
lv_obj_t *primary, *finish, *confirm, *cancel, *task[2];

void Show(lv_obj_t* obj, bool show) {
  if (show && lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (!show && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
void Text(lv_obj_t* obj, const char* value) {
  if (std::string_view(lv_label_get_text(obj)) != value) lv_label_set_text(obj, value);
}
void Color(lv_obj_t* obj, uint32_t color) {
  const auto value = lv_color_hex(color);
  if (!lv_color_eq(lv_obj_get_style_bg_color(obj, LV_PART_MAIN), value))
    lv_obj_set_style_bg_color(obj, value, LV_PART_MAIN);
}
void Refresh() {
  auto& r = *runtime;
  const auto& state = r.coordinator.state();
  ui::ViewState view;
  view.domain.active_session = state.active_session;
  const auto focus = ui::buildFocus(view, r.clock.monotonicMs());
  if (r.cue_session != focus.session_id) {
    r.cue_session = focus.session_id;
    r.cue_played = false;
  }
  const bool due = focus.running && focus.timeout_prompt;
  auto& app = Application::GetInstance();
  if (due && !r.cue_played && !Claw4TimerPreviewIsRinging() &&
      app.GetDeviceState() == kDeviceStateIdle && app.GetAudioService().IsPlaybackIdle()) {
    app.PlaySound(Lang::Sounds::OGG_VIBRATION);
    r.cue_played = true;
    ESP_LOGI(kTag, "LOCAL_FOCUS_CUE");
  }
  const auto now = Claw4TimerPreviewTrustedTime();
  if (!lvgl_port_lock(100)) return;
  if (Claw4TimerPreviewIsRinging()) Show(panel, false);
  Text(counter, focus.has_session ? focus.countdown_text.c_str() : "01:00");
  const int percent = focus.has_session && focus.planned_minutes > 0
      ? int(100 - focus.remaining_ms * 100 / (int64_t(focus.planned_minutes) * 60000)) : 0;
  if (lv_bar_get_value(progress) != percent) lv_bar_set_value(progress, percent, LV_ANIM_OFF);
  Text(state_label, due ? "Time is up" : focus.running ? "Focusing" : focus.has_session ? "Paused" : "Choose a task");
  Text(feedback_label, !r.ready ? "Learning storage unavailable" : !now ? "Waiting for device clock" :
       r.confirming && !*r.feedback ? "Confirm completion on this screen." : r.feedback);
  Text(lv_obj_get_child(primary, 0), focus.running ? "Pause" : focus.has_session ? "Resume" : "Start focus");
  Show(primary, r.ready && now && !r.confirming);
  Show(finish, r.ready && now && focus.has_session && !r.confirming);
  Show(confirm, r.confirming && now);
  Show(cancel, r.confirming);
  for (int i = 0; i < 2; ++i) {
    const std::string_view id = i == 0 ? "math" : "reading";
    Color(task[i], (focus.has_session ? focus.task_id : r.selection).value == id ? kMint : kWhite);
    const char* title = i == 0 ? "Math warm-up" : "Reading break";
    for (const auto& item : state.tasks) {
      if (item.task_id.value == id && item.status == domain::TaskStatus::Completed)
        title = i == 0 ? "Math completed" : "Reading completed";
    }
    Text(lv_obj_get_child(task[i], 0), title);
  }
  lvgl_port_unlock();
}

void Handle(Tap tap) {
  if (!runtime || Claw4TimerPreviewIsRinging()) return;
  auto& r = *runtime;
  if (tap == Tap::Open || tap == Tap::Close) {
    if (lvgl_port_lock(100)) {
      Show(panel, tap == Tap::Open);
      if (tap == Tap::Open) lv_obj_move_foreground(panel);
      lvgl_port_unlock();
    }
    if (tap == Tap::Close) {
      r.commands.cancelConfirmation();
      r.confirming = false;
    }
  } else if (r.ready) {
    const auto& state = r.coordinator.state();
    const auto now = Claw4TimerPreviewTrustedTime();
    if (tap == Tap::Math || tap == Tap::Reading) {
      if (!state.active_session) {
        r.selection = {tap == Tap::Math ? "math" : "reading"};
        r.feedback = "";
      }
    } else if (tap == Tap::Cancel) {
      r.commands.cancelConfirmation();
      r.confirming = false;
      r.feedback = "";
    } else if (now && tap == Tap::Confirm && r.confirming) {
      const auto reply = r.commands.confirmFromTouch(binding, r.context(*now), now);
      r.confirming = reply.status == Status::StorageError;
      r.feedback = reply.status == Status::Applied ? "Task saved on Claw4." :
                   r.confirming ? "Could not save. Try confirming again." : "Confirmation expired.";
      ESP_LOGI(kTag, "LOCAL_CONFIRM outcome=%d", int(reply.status));
    } else if (now && !r.confirming && (tap == Tap::StartPause || tap == Tap::Finish)) {
      Kind kind = Kind::StartTask;
      if (tap == Tap::Finish) kind = Kind::CompleteRequest;
      else if (state.active_session) kind = state.active_session->status == domain::SessionStatus::Running
          ? Kind::PauseTask : Kind::ResumeTask;
      const application::LearningVoiceRequest request{
          Id("tap"), binding.session_epoch, kind,
          state.active_session ? state.active_session->task_id : r.selection, {}, *now + 60000};
      const auto reply = r.commands.execute(request, binding, r.context(*now), now, {}, {});
      r.confirming = reply.status == Status::ConfirmationRequired;
      r.feedback = reply.status == Status::Applied || r.confirming ? "" :
          reply.status == Status::StorageError ? "Could not save. Try again." : "This task is unavailable.";
      ESP_LOGI(kTag, "LOCAL_ACTION kind=%d outcome=%d", int(kind), int(reply.status));
    }
  }
  Refresh();
}

void Surface(lv_obj_t* obj, uint32_t color, int radius) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}
lv_obj_t* Label(lv_obj_t* parent, const char* title, int x, int y, uint32_t color, bool large = false) {
  auto* label = lv_label_create(parent);
  lv_label_set_text(label, title);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  if (large) lv_obj_set_style_text_font(label, &font_noto_sans_basic_30_4, LV_PART_MAIN);
  return label;
}
lv_obj_t* Button(lv_obj_t* parent, const char* title, Tap tap,
                 int x, int y, int width, int height, uint32_t color, bool large = false) {
  auto* button = lv_button_create(parent);
  Surface(button, color, 18);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);
  lv_obj_center(Label(button, title, 0, 0, kInk, large));
  lv_obj_add_event_cb(button, [](lv_event_t* e) {
    const auto tap = static_cast<Tap>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    Application::GetInstance().Schedule([tap] { Handle(tap); });
  }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(tap)));
  return button;
}
}  // namespace

void Claw4LearningPreviewSetup(lv_obj_t* parent) {
  if (panel || !lvgl_port_lock(1000)) return;
  Button(parent, "Study", Tap::Open, 548, 92, 154, 50, kMint);
  panel = lv_obj_create(parent);
  Surface(panel, kPaper, 32);
  lv_obj_set_size(panel, 610, 515);
  lv_obj_center(panel);
  Label(panel, "Study corner", 28, 23, kInk, true);
  Label(panel, "Local preview  /  Sample tasks", 29, 64, kMuted);
  Button(panel, "Close", Tap::Close, 488, 27, 94, 48, kLilac);
  task[0] = Button(panel, "Math warm-up", Tap::Math, 28, 108, 270, 64, kMint, true);
  task[1] = Button(panel, "Reading break", Tap::Reading, 310, 108, 272, 64, kWhite, true);
  auto* card = lv_obj_create(panel);
  Surface(card, kLilac, 24);
  lv_obj_set_pos(card, 28, 192);
  lv_obj_set_size(card, 554, 144);
  state_label = Label(card, "Choose a task", 25, 20, kMuted);
  counter = Label(card, "01:00", 24, 50, kInk, true);
  lv_obj_set_style_transform_scale_x(counter, 390, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_y(counter, 390, LV_PART_MAIN);
  progress = lv_bar_create(card);
  lv_obj_set_pos(progress, 25, 112);
  lv_obj_set_size(progress, 504, 9);
  lv_obj_set_style_bg_color(progress, lv_color_hex(0xD2C3E5), LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress, lv_color_hex(0x468675), LV_PART_INDICATOR);
  feedback_label = Label(panel, "", 29, 357, kMuted);
  primary = Button(panel, "Start focus", Tap::StartPause, 28, 407, 270, 62, kMint, true);
  finish = Button(panel, "Finish task", Tap::Finish, 310, 407, 272, 62, kWhite, true);
  confirm = Button(panel, "Confirm", Tap::Confirm, 28, 407, 270, 62, kMint, true);
  cancel = Button(panel, "Keep going", Tap::Cancel, 310, 407, 272, 62, kWhite, true);
  Show(panel, false);
  lv_timer_create([](lv_timer_t*) {
    if (refresh_pending.exchange(true)) return;
    Application::GetInstance().Schedule([] {
      if (!runtime) runtime = std::make_unique<Runtime>();
      Refresh();
      refresh_pending.store(false);
    });
  }, 500, nullptr);
  lvgl_port_unlock();
}
#endif
