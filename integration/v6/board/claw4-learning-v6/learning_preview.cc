#include "learning_preview.h"

#ifdef CLAW4_LEARNING_PREVIEW
#include "application.h"
#include "application/learning_voice_commands.h"
#include "assets/lang_config.h"
#include "learning_today.h"
#include "learning_voice_mcp.h"
#include "metalio_claw4/device/ports/nvs_backend_provisioning.h"
#include "metalio_claw4/device/ports/nvs_outbox_storage.h"
#include "timer_preview.h"

#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <algorithm>
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

sync::ProvisionedBackendConfig ReadPairing() {
  sync::ProvisionedBackendConfig config;
  metalio::NvsBackendProvisioning provisioning;
  if (provisioning.load(config) != sync::ProvisioningStatus::Ready ||
      config.device_secret.size() != 64 ||
      config.device_secret.find_first_not_of("0123456789abcdef") != std::string::npos) return {};
  return config;
}

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
  sync::ProvisionedBackendConfig pairing = ReadPairing();
  const bool paired = pairing.complete();
  const application::LearningVoiceBinding identity = paired
      ? application::LearningVoiceBinding{{pairing.device_id}, {pairing.child_id}, 0} : binding;
  Clock clock;
  time::TimeAuthority calendar{clock, "local-preview"};
  NoReminderStorage reminder_store;
  reminder::ReminderCore reminders{{}, reminder_store, calendar};
  metalio::NvsOutboxStorage storage{paired ? "claw4_v6study" : "claw4_v6demo"};
  domain::DomainReducer reducer;
  application::AppCoordinator coordinator{storage, reducer, {.allow_baseline_rebase = false}};
  application::LearningVoiceCommands commands{coordinator, reminders};
  domain::TaskId selection{"math"};
  domain::SessionId cue_session;
  bool ready = false, confirming = false, cue_played = false;
  std::string voice_connection;
  int64_t voice_epoch = 0;
  bool voice_confirmation = false;
  bool backend_authorized = false;
  bool worker_started = false;
  bool upload_inflight = false;
  bool upload_blocked = false;
  bool records_pending = false;
  int64_t next_upload_ms = 0;
  std::optional<int64_t> snapshot_epoch;
  std::string snapshot_date;
  const char* feedback = "";

  Runtime() {
    sync::OutboxState previous;
    const auto presence = storage.loadWithPresence(previous);
    const bool snapshot_owned = !paired ||
        (std::all_of(previous.domain.tasks.begin(), previous.domain.tasks.end(), [&](const auto& task) {
           return task.child_id == identity.child_id;
         }) && (!previous.domain.active_session ||
                (previous.domain.active_session->device_id == identity.device_id &&
                 previous.domain.active_session->child_id == identity.child_id)) &&
         std::all_of(previous.pending.begin(), previous.pending.end(), [&](const auto& row) {
           return row.device_id == identity.device_id && row.child_id == identity.child_id;
         }));
    ready = presence != metalio::NvsOutboxStorage::StateReadStatus::Error && snapshot_owned &&
            coordinator.prepareAfterBoot(clock.monotonicMs());
    if (ready && !paired && presence == metalio::NvsOutboxStorage::StateReadStatus::Missing) {
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
    return {identity.device_id, identity.child_id, now / 1000, clock.monotonicMs(),
            [] { return domain::EventId{Id("event")}; },
            [] { return domain::SessionId{Id("study")}; }};
  }
  void RefreshConnection() {
    auto next = Application::GetInstance().LearningConnectionId();
    if (next == voice_connection) return;
    voice_connection = std::move(next);
    if (voice_confirmation) {
      commands.cancelConfirmation();
      confirming = voice_confirmation = false;
    }
    if (voice_connection.empty()) {
      voice_epoch = 0;
    } else {
      const uint64_t random = (uint64_t(esp_random()) << 32) | esp_random();
      voice_epoch = int64_t((random & 0x3ffffffffffffffeULL) + 2);
    }
  }
  application::LearningVoiceBinding TouchBinding() const {
    return {identity.device_id, identity.child_id, voice_confirmation ? voice_epoch : 1};
  }
  std::optional<int64_t> Today(int64_t now) const {
    return snapshot_epoch && *snapshot_epoch <= now && v6::LearningTodayDateMatches(snapshot_date, now)
        ? snapshot_epoch : std::nullopt;
  }
  void ApplyToday(v6::LearningTodayUpdate update) {
    if (!update.snapshot) {
      backend_authorized = false;
      snapshot_epoch.reset();
      if (voice_confirmation) {
        commands.cancelConfirmation();
        confirming = voice_confirmation = false;
      }
      return;
    }
    auto& today = *update.snapshot;
    const auto now = Claw4TimerPreviewTrustedTime();
    if (!now || !v6::LearningTodayMatches(today, identity.child_id, *now)) return;
    if (!backend_authorized) {
      coordinator.resetAuthPause();
      next_upload_ms = 0;
    }
    backend_authorized = true;
    // An authenticated device can drain its queue even if a larger snapshot
    // cannot fit until pending events have been acknowledged.
    if (!coordinator.applyTodaySnapshot(today.tasks)) return;
    snapshot_date = std::move(today.date);
    snapshot_epoch = now;
  }
  void TickUpload() {
    if (!worker_started || upload_inflight || clock.monotonicMs() < next_upload_ms)
      return;
    next_upload_ms = clock.monotonicMs() + 2'000;
    records_pending = coordinator.pendingCount() > 0;
    if (!records_pending || !backend_authorized) return;
    auto outgoing = coordinator.prepareSync(8);
    if (outgoing.auth_paused || outgoing.request.events.empty()) return;
    if (outgoing.request.device_id != identity.device_id ||
        !std::all_of(outgoing.request.events.begin(), outgoing.request.events.end(), [&](const auto& row) {
          return row.device_id == identity.device_id && row.child_id == identity.child_id;
        })) {
      upload_blocked = true;
      next_upload_ms = clock.monotonicMs() + 60'000;
      return;
    }
    upload_inflight = v6::QueueLearningUpload(std::move(outgoing));
  }
  void UploadFinished(application::SyncRequestEnvelope sent, sync::SyncClient::Response result) {
    upload_inflight = false;
    const auto before = coordinator.lastAcked();
    const auto applied = coordinator.applySyncResult(sent, result);
    const auto after = coordinator.lastAcked();
    records_pending = coordinator.pendingCount() > 0;
    upload_blocked = applied.outcome == application::SyncOutcome::Blocked;
    if (after > before && applied.outcome == application::SyncOutcome::Synced) {
      next_upload_ms = 0;
      ESP_LOGI(kTag, "LEARNING_UPLOAD_ACK count=%d", int(after - before));
    } else {
      const auto wait = std::max<int64_t>(2'000, coordinator.backoffDelayMs());
      next_upload_ms = clock.monotonicMs() + (applied.outcome == application::SyncOutcome::Blocked ? 30'000 : wait);
    }
  }
};

enum class Tap { Open, Close, SelectFirst, SelectSecond, StartPause, Finish, Confirm, Cancel, Next };
std::unique_ptr<Runtime> runtime;
std::atomic<bool> refresh_pending{false};
lv_obj_t *panel, *counter, *progress, *state_label, *feedback_label, *subtitle;
lv_obj_t *primary, *finish, *confirm, *cancel, *next_tasks, *task[2];
std::size_t task_page = 0;

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
  if (r.paired) r.RefreshConnection();
  r.TickUpload();
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
  if (r.confirming && !r.commands.confirmationPending(now.value_or(0))) {
    r.commands.cancelConfirmation();
    r.confirming = r.voice_confirmation = false;
    r.feedback = "Confirmation expired.";
  }
  if (!lvgl_port_lock(100)) return;
  if (Claw4TimerPreviewIsRinging()) Show(panel, false);
  Text(subtitle, r.paired ? "Paired learning" : "Local preview  /  Sample tasks");
  Text(counter, focus.has_session ? focus.countdown_text.c_str() : "01:00");
  const bool current_today = !r.paired || (now && r.Today(*now));
  const auto shown_count = current_today ? state.tasks.size() : 0;
  if (task_page >= shown_count) task_page = 0;
  const int percent = focus.has_session && focus.planned_minutes > 0
      ? int(100 - focus.remaining_ms * 100 / (int64_t(focus.planned_minutes) * 60000)) : 0;
  if (lv_bar_get_value(progress) != percent) lv_bar_set_value(progress, percent, LV_ANIM_OFF);
  Text(state_label, due ? "Time is up" : focus.running ? "Focusing" : focus.has_session ? "Paused" :
       !current_today ? "Waiting for tasks" : shown_count == 0 ? "No tasks today" : "Choose a task");
  Text(feedback_label, !r.ready ? "Learning storage unavailable" : !now ? "Waiting for device clock" :
       r.confirming && !*r.feedback ? "Confirm completion on this screen." :
       r.upload_blocked ? "Learning records need review." :
       r.records_pending && !*r.feedback ? "Records saved on Claw4. Sync pending." : r.feedback);
  Text(lv_obj_get_child(primary, 0), focus.running ? "Pause" : focus.has_session ? "Resume" : "Start focus");
  const bool selected_ready = current_today &&
      std::any_of(state.tasks.begin(), state.tasks.end(), [&](const auto& item) {
        return item.task_id == r.selection && item.status == domain::TaskStatus::Ready;
      });
  Show(primary, r.ready && now && !r.confirming && (focus.has_session || selected_ready));
  Show(finish, r.ready && now && focus.has_session && !r.confirming);
  Show(confirm, r.confirming && now);
  Show(cancel, r.confirming);
  Show(next_tasks, shown_count > 2 && !r.confirming);
  for (int i = 0; i < 2; ++i) {
    const auto index = task_page + i;
    Show(task[i], index < shown_count);
    if (index >= shown_count) continue;
    const auto& item = state.tasks[index];
    const auto title = item.status == domain::TaskStatus::Completed ? item.title + "  (Done)" : item.title;
    Color(task[i], (focus.has_session ? focus.task_id : r.selection) == item.task_id ? kMint : kWhite);
    Text(lv_obj_get_child(task[i], 0), title.c_str());
  }
  lvgl_port_unlock();
}

void Handle(Tap tap) {
  if (!runtime || Claw4TimerPreviewIsRinging()) return;
  auto& r = *runtime;
  if (r.paired) r.RefreshConnection();
  if (tap == Tap::Open || tap == Tap::Close) {
    if (lvgl_port_lock(100)) {
      Show(panel, tap == Tap::Open);
      if (tap == Tap::Open) lv_obj_move_foreground(panel);
      lvgl_port_unlock();
    }
    if (tap == Tap::Close) {
      r.commands.cancelConfirmation();
      r.confirming = r.voice_confirmation = false;
    }
  } else if (r.ready) {
    const auto& state = r.coordinator.state();
    const auto now = Claw4TimerPreviewTrustedTime();
    const bool current_today = !r.paired || (now && r.Today(*now));
    if (tap == Tap::Next && current_today) {
      task_page = task_page + 2 < state.tasks.size() ? task_page + 2 : 0;
    } else if (tap == Tap::SelectFirst || tap == Tap::SelectSecond) {
      const auto index = task_page + (tap == Tap::SelectSecond);
      if (current_today && !r.confirming && !state.active_session && index < state.tasks.size()) {
        r.selection = state.tasks[index].task_id;
        r.feedback = "";
      }
    } else if (tap == Tap::Cancel) {
      r.commands.cancelConfirmation();
      r.confirming = r.voice_confirmation = false;
      r.feedback = "";
    } else if (now && tap == Tap::Confirm && r.confirming) {
      const auto reply = r.commands.confirmFromTouch(r.TouchBinding(), r.context(*now), now);
      r.confirming = reply.status == Status::StorageError;
      if (!r.confirming) r.voice_confirmation = false;
      r.feedback = reply.status == Status::Applied ? "Task saved on Claw4." :
                   r.confirming ? "Could not save. Try confirming again." : "Confirmation expired.";
      ESP_LOGI(kTag, "LOCAL_CONFIRM outcome=%d", int(reply.status));
    } else if (now && !r.confirming && (state.active_session || current_today) &&
               (tap == Tap::StartPause || tap == Tap::Finish)) {
      const auto task_id = state.active_session ? state.active_session->task_id : r.selection;
      auto status = Status::InvalidState;
      if (tap == Tap::Finish) {
        const application::LearningVoiceRequest request{
            Id("tap"), 1, Kind::CompleteRequest, task_id, {}, *now + 60000};
        status = r.commands.execute(request, r.TouchBinding(), r.context(*now), now, {}, {}).status;
      } else {
        domain::Intent intent = domain::Intent::StartTask;
        std::optional<domain::SessionId> session_id;
        if (state.active_session) {
          session_id = state.active_session->session_id;
          intent = state.active_session->status == domain::SessionStatus::Running
              ? domain::Intent::Pause : domain::Intent::Resume;
        }
        const auto result = r.coordinator.dispatchIntent({intent, task_id, session_id}, r.context(*now));
        if (result.ok) status = Status::Applied;
        else if (result.intent_result == domain::IntentResult::PersistFailed) status = Status::StorageError;
      }
      r.confirming = status == Status::ConfirmationRequired;
      r.feedback = status == Status::Applied || r.confirming ? "" :
          status == Status::StorageError ? "Could not save. Try again." : "This task is unavailable.";
      ESP_LOGI(kTag, "LOCAL_ACTION tap=%d outcome=%d", int(tap), int(status));
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
  subtitle = Label(panel, "Local preview  /  Sample tasks", 29, 64, kMuted);
  Button(panel, "Close", Tap::Close, 488, 27, 94, 48, kLilac);
  task[0] = Button(panel, "Math warm-up", Tap::SelectFirst, 28, 108, 270, 64, kMint, true);
  task[1] = Button(panel, "Reading break", Tap::SelectSecond, 310, 108, 272, 64, kWhite, true);
  for (auto* button : task) {
    auto* label = lv_obj_get_child(button, 0);
    lv_obj_set_width(label, 238);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  }
  next_tasks = Button(panel, "More", Tap::Next, 496, 350, 86, 39, kLilac);
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
      if (!runtime) {
        runtime = std::make_unique<Runtime>();
        auto& r = *runtime;
        if (r.paired && r.ready) {
          r.worker_started = v6::StartLearningTodayWorker(r.pairing, [](v6::LearningTodayUpdate update) {
            runtime->ApplyToday(std::move(update));
          }, [](application::SyncRequestEnvelope sent, sync::SyncClient::Response result) {
            runtime->UploadFinished(std::move(sent), std::move(result));
          });
          if (!r.worker_started) r.feedback = "Task sync unavailable.";
          v6::RegisterLearningVoiceTool(r.commands, v6::CreateLearningVoiceSigner(std::move(r.pairing.device_secret)),
              []() -> std::optional<v6::LearningVoiceCallContext> {
                auto& current = *runtime;
                current.RefreshConnection();
                const auto now = Claw4TimerPreviewTrustedTime();
                if (!current.backend_authorized || !now || !current.voice_epoch ||
                    Claw4TimerPreviewIsRinging()) return std::nullopt;
                return v6::LearningVoiceCallContext{
                    {current.identity.device_id, current.identity.child_id, current.voice_epoch},
                    current.voice_connection, current.context(*now), now, current.Today(*now), {}};
              }, [] {
                runtime->confirming = runtime->voice_confirmation = true;
                runtime->feedback = "";
                Handle(Tap::Open);
              });
        }
      }
      Refresh();
      refresh_pending.store(false);
    });
  }, 500, nullptr);
  lvgl_port_unlock();
}
#endif
