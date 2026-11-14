#include "timer_preview.h"
#include "timer_preview_cue.h"
#include "sdkconfig.h"

#ifdef CONFIG_CLAW4_TIMER_PREVIEW
#include "application.h"
#include "device_state.h"
#include "mcp_server.h"
#include "reminder/reminder_core.h"
#include "time/time_authority.h"
#include "assets/lang_config.h"
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>

LV_FONT_DECLARE(font_noto_sans_basic_30_4);

namespace {
constexpr int64_t kDay = 86400000;
constexpr int64_t kChina = 8 * 3600000;
constexpr char kNamespace[] = "claw4_tmr";
constexpr char kKey[] = "alarm";
constexpr char kChild[] = "timer_preview";
constexpr int kAcknowledgeRinging = 11;
constexpr int kOneMinuteFocus = 12;
constexpr int kToggleFocusPause = 13;
constexpr uint32_t kSurface = 0x121F29;
constexpr uint32_t kCard = 0x1B2D39;
constexpr uint32_t kText = 0xF1F6F5;
constexpr uint32_t kMuted = 0xB3C4C9;
constexpr uint32_t kMint = 0x8DE2BF;
constexpr uint32_t kAmber = 0xFFD18F;
constexpr uint32_t kRed = 0xB62D28;

class Clock final : public claw4::ports::ClockPort {
public:
    int64_t epochSeconds() override { return 0; }
    int64_t monotonicMs() override { return esp_timer_get_time() / 1000; }
    bool isTimeSynced() override { return false; }
};

class Store final : public claw4::reminder::ReminderStore {
public:
    bool load(std::vector<claw4::reminder::ReminderRecord>& out) override {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
        if (err == ESP_ERR_NVS_NOT_FOUND) return true;
        if (err != ESP_OK) return false;
        int64_t value = 0;
        err = nvs_get_i64(handle, kKey, &value);
        nvs_close(handle);
        if (err == ESP_ERR_NVS_NOT_FOUND) return true;
        if (err != ESP_OK) return false;
        if (value == 0) return true;
        if (value == std::numeric_limits<int64_t>::min() ||
            std::llabs(value) < 1600000000000LL || std::llabs(value) > 4102444800000LL) return false;
        claw4::reminder::ReminderRecord record;
        record.kind = claw4::reminder::ReminderKind::ManualAlarm;
        record.child_id = claw4::domain::ChildId{kChild};
        record.due_epoch_ms = std::llabs(value);
        record.instance_id = claw4::reminder::ReminderCore::makeInstanceId(
            record.kind, record.child_id, record.task_id, record.due_epoch_ms);
        if (value < 0) { record.active = false; record.delivered_epoch_ms = record.due_epoch_ms; }
        out.push_back(record);
        return true;
    }
    bool save(const std::vector<claw4::reminder::ReminderRecord>& records) override {
        int64_t value = 0;
        for (const auto& record : records) {
            if (record.kind != claw4::reminder::ReminderKind::ManualAlarm) return false;
            if (record.active && record.delivered_epoch_ms == 0) value = record.due_epoch_ms;
            else if (record.delivered_epoch_ms > 0) value = -record.due_epoch_ms;
        }
        nvs_handle_t handle;
        if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
        esp_err_t err = nvs_set_i64(handle, kKey, value);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
        return err == ESP_OK;
    }
};

Clock clock_port;
Store store;
// Preview policy bound, not a measured crystal-drift guarantee.
claw4::time::TimeAuthority authority(clock_port, "claw4_timer_preview",
    claw4::time::TimeAuthorityConfig{3600000, 86400000, 60000, 500});
claw4::reminder::ReminderPolicy policy = [] {
    claw4::reminder::ReminderPolicy p;
    p.max_instances = p.max_records = p.max_deliveries_per_tick = p.max_batches_per_tick = 1;
    return p;
}();
claw4::reminder::ReminderCore reminders(policy, store, authority);
std::mutex state_mutex;
std::atomic<bool> tick_queued{false};
bool storage_ok = false;
bool running = false;
bool paused = false;
bool focus_done = false;
int64_t deadline_ms = 0;
int64_t remaining_ms = 0;
int64_t duration_ms = 0;
int64_t alarm_due = 0;
int64_t alarm_retry_after = 0;
bool alarm_visual = false;
bool focus_ringing = false;
bool alarm_ringing = false;
std::string acknowledged_alarm;
claw4::TimerPreviewCue ringer;
std::atomic<bool> ringing{false};
lv_obj_t* panel = nullptr;
lv_obj_t* entry = nullptr;
lv_obj_t* entry_label = nullptr;
lv_obj_t* focus_label = nullptr;
lv_obj_t* focus_state = nullptr;
lv_obj_t* focus_progress = nullptr;
lv_obj_t* alarm_label = nullptr;
lv_obj_t* focus_presets[3]{};
lv_obj_t* focus_pause = nullptr;
lv_obj_t* focus_pause_label = nullptr;
lv_obj_t* focus_cancel = nullptr;
lv_obj_t* alarm_set = nullptr;
lv_obj_t* alarm_cancel = nullptr;
lv_obj_t* close_panel = nullptr;
lv_obj_t* stop_alarm = nullptr;
bool shown_ringing = false;

int64_t Now() { return clock_port.monotonicMs(); }
int64_t Remaining() {
    const int64_t left = running && !paused ? deadline_ms - Now() : remaining_ms;
    return left > 0 ? left : 0;
}
int64_t AlarmDue() {
    for (const auto& r : reminders.records())
        if (r.kind == claw4::reminder::ReminderKind::ManualAlarm && r.active && !r.delivered_epoch_ms)
            return r.due_epoch_ms;
    return 0;
}
void SetText(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}
void Enable(lv_obj_t* button, bool enabled) {
    if (enabled && lv_obj_has_state(button, LV_STATE_DISABLED))
        lv_obj_remove_state(button, LV_STATE_DISABLED);
    else if (!enabled && !lv_obj_has_state(button, LV_STATE_DISABLED))
        lv_obj_add_state(button, LV_STATE_DISABLED);
}
void Render() {
    if (!panel) return;
    char focus[16], alarm[64], chip[48];
    const char* status_text;
    bool ring_visible, is_running, is_paused, can_set_alarm, can_cancel_alarm;
    int progress;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ring_visible = ringer.active();
        is_running = running;
        is_paused = paused;
        const int64_t left = Remaining();
        const int seconds = static_cast<int>((left + 999) / 1000);
        progress = duration_ms > 0 ?
            static_cast<int>(std::clamp((duration_ms - left) * 1000 / duration_ms, int64_t{0}, int64_t{1000})) : 0;
        std::snprintf(focus, sizeof(focus), "%02d:%02d", seconds / 60, seconds % 60);
        status_text = focus_ringing ? "Ringing" : focus_done ? "Complete" : paused ? "Paused" : running ? "Focusing" : "Ready";
        if (ring_visible) std::snprintf(chip, sizeof(chip), "Alarm ringing");
        else if (alarm_visual) std::snprintf(chip, sizeof(chip), "Alarm done");
        else if (focus_done) std::snprintf(chip, sizeof(chip), "Focus done");
        else if (running) std::snprintf(chip, sizeof(chip), "%s %s", paused ? "Paused" : "Focus", focus);
        else std::snprintf(chip, sizeof(chip), "Timer / Alarm");
        const auto status = authority.status();
        can_set_alarm = storage_ok && status.calendar_allowed && !alarm_due;
        can_cancel_alarm = storage_ok && alarm_due && acknowledged_alarm.empty();
        if (alarm_ringing) std::snprintf(alarm, sizeof(alarm), "Ringing now");
        else if (!storage_ok) std::snprintf(alarm, sizeof(alarm), "Storage unavailable");
        else if (!status.calendar_allowed) std::snprintf(alarm, sizeof(alarm), "Waiting for clock");
        else if (alarm_visual) std::snprintf(alarm, sizeof(alarm), "Alarm acknowledged");
        else if (alarm_due) {
            int64_t local = (alarm_due + kChina) / 60000;
            std::snprintf(alarm, sizeof(alarm), "%02lld:%02lld  China time", (long long)((local / 60) % 24), (long long)(local % 60));
        } else std::snprintf(alarm, sizeof(alarm), "No alarm set");
    }
    if (!lvgl_port_lock(100)) return;
    SetText(entry_label, chip);
    SetText(focus_label, focus);
    SetText(focus_state, status_text);
    SetText(alarm_label, alarm);
    SetText(focus_pause_label, is_paused ? "Resume" : "Pause");
    if (lv_bar_get_value(focus_progress) != progress) lv_bar_set_value(focus_progress, progress, LV_ANIM_OFF);
    for (auto* preset : focus_presets) Enable(preset, !ring_visible && !is_running);
    Enable(focus_pause, !ring_visible && is_running);
    Enable(focus_cancel, !ring_visible && is_running);
    Enable(alarm_set, !ring_visible && can_set_alarm);
    Enable(alarm_cancel, !ring_visible && can_cancel_alarm);
    Enable(close_panel, !ring_visible);
    if (ring_visible != shown_ringing) {
        if (ring_visible) {
            lv_obj_set_height(panel, 566);
            lv_obj_remove_flag(stop_alarm, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(stop_alarm, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_height(panel, 496);
        }
        lv_obj_set_style_bg_color(entry, lv_color_hex(ring_visible ? kRed : kCard), LV_PART_MAIN);
        shown_ringing = ring_visible;
    }
    lvgl_port_unlock();
}
const char* Action(int action, int minutes, int hour, int minute, int day_offset) {
    std::lock_guard<std::mutex> lock(state_mutex);
    const int64_t now = Now();
    if (action == kToggleFocusPause) action = paused ? 2 : 1;
    switch (action) {
    case 0:
        if (minutes < 1 || minutes > 120) return "invalid_time";
        if (running || focus_ringing) return "busy";
        duration_ms = remaining_ms = int64_t(minutes) * 60000;
        deadline_ms = now + remaining_ms;
        running = true; paused = false; focus_done = false;
        ESP_LOGI("Claw4Timer", "FOCUS started minutes=%d", minutes);
        return "ok";
    case 1:
        if (!running || paused) return "inactive";
        remaining_ms = Remaining(); paused = true; return "ok";
    case 2:
        if (!running || !paused) return "inactive";
        deadline_ms = now + remaining_ms; paused = false; return "ok";
    case 3:
        if (focus_ringing) return "busy";
        running = paused = focus_done = false; remaining_ms = duration_ms = 0; return "ok";
    case 4: {
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || day_offset < 0 || day_offset > 1) return "invalid_time";
        if (!storage_ok) return "not_saved";
        if (AlarmDue()) return "busy";
        auto status = authority.status();
        if (!status.calendar_allowed || !status.epoch_ms) return "clock_not_ready";
        const int64_t local = *status.epoch_ms + kChina;
        const int64_t day_start = (local / kDay) * kDay;
        const int64_t due = day_start + int64_t(day_offset) * kDay + int64_t(hour) * 3600000 + int64_t(minute) * 60000 - kChina;
        if (due <= *status.epoch_ms) return "invalid_time";
        auto result = reminders.setAlarm(claw4::domain::ChildId{kChild}, due);
        if (result == claw4::reminder::AlarmScheduleResult::ClockNotReady) return "clock_not_ready";
        if (result == claw4::reminder::AlarmScheduleResult::InvalidTime) return "invalid_time";
        if (result == claw4::reminder::AlarmScheduleResult::NotSaved) return "not_saved";
        alarm_due = due; alarm_visual = false; return "ok";
    }
    case 5:
        if (!storage_ok) return "not_saved";
        if (alarm_ringing || !acknowledged_alarm.empty()) return "busy";
        if (!AlarmDue()) return "inactive";
        if (!reminders.cancelAlarm(claw4::domain::ChildId{kChild}, AlarmDue())) return "not_saved";
        alarm_due = 0; alarm_visual = false; return "ok";
    case 6: return "ok";
    default: return "invalid_time";
    }
}
void AcknowledgeRinging() {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!ringer.active()) return;
    if (alarm_ringing) {
        // Use the same record identity as ReminderCore. Mute immediately and
        // retry persistence independently if the first NVS write fails.
        acknowledged_alarm = claw4::reminder::ReminderCore::makeInstanceId(
            claw4::reminder::ReminderKind::ManualAlarm, claw4::domain::ChildId{kChild}, {}, alarm_due);
        alarm_retry_after = 0;
    }
    focus_ringing = alarm_ringing = false;
    ringer.Acknowledge();
    ESP_LOGI("Claw4Timer", "RINGER acknowledged");
}
void Tick() {
    bool ring_active;
    const int64_t now = Now();
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (running && !paused && deadline_ms <= now) {
            running = false; remaining_ms = 0; focus_done = true;
            focus_ringing = true;
            ESP_LOGI("Claw4Timer", "FOCUS due");
            ringer.Arm(now);
        }
        if (storage_ok) {
            alarm_due = AlarmDue();
            if (!acknowledged_alarm.empty()) {
                if (now >= alarm_retry_after) {
                    if (reminders.markDelivered(acknowledged_alarm, authority.status().epoch_ms.value_or(alarm_due))) {
                        acknowledged_alarm.clear();
                        alarm_due = 0;
                    } else {
                        alarm_retry_after = now + 5000;
                    }
                }
            } else if (alarm_due && !alarm_ringing && !reminders.collectDue(0).empty()) {
                alarm_visual = alarm_ringing = true;
                ringer.Arm(now);
            }
        }
        ring_active = ringer.active();
        if (ring_active && !ringing.exchange(true)) ESP_LOGI("Claw4Timer", "RINGER active");
    }
    auto& app = Application::GetInstance();
    if (ring_active && app.PrepareTimerAlarmPlayback()) {
        bool play;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            play = ringer.Poll(now, true);
        }
        if (play) {
            app.PlaySound(Lang::Sounds::OGG_VIBRATION);
            ESP_LOGI("Claw4Timer", "RINGER clip");
        }
    } else if (!ring_active && ringing.load() && app.GetAudioService().IsPlaybackIdle()) {
        ringing.store(false);
        if (app.GetDeviceState() == kDeviceStateIdle && !app.IsChatInterrupted())
            app.GetAudioService().EnableWakeWordDetection(true);
        ESP_LOGI("Claw4Timer", "RINGER released");
    }
    Render();
    tick_queued = false;
}
void TickTask(void*) {
    for (;;) {
        if (!tick_queued.exchange(true)) Application::GetInstance().Schedule([] { Tick(); });
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
void Click(lv_event_t* event) {
    const int action = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    if (action == 7) { lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN); return; }
    if (action == 8) { lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN); return; }
    Application::GetInstance().Schedule([action] {
        if (action == kAcknowledgeRinging) {
            AcknowledgeRinging();
        } else if (action == 9) {
            int hour = 0, minute = 0, offset = 0;
            { std::lock_guard<std::mutex> lock(state_mutex);
              auto status = authority.status();
              if (status.calendar_allowed && status.epoch_ms) {
                  int64_t local_minute = (*status.epoch_ms + kChina) / 60000 + 1;
                  hour = (local_minute / 60) % 24;
                  minute = local_minute % 60;
                  offset = ((local_minute * 60000) / kDay) - ((*status.epoch_ms + kChina) / kDay);
              }
            }
            Action(4, 25, hour, minute, offset);
        } else if (action == 10) Action(0, 5, 0, 0, 0);
        else if (action == kOneMinuteFocus) Action(0, 1, 0, 0, 0);
        else Action(action, 25, 0, 0, 0);
        Render();
    });
}
lv_obj_t* Label(lv_obj_t* parent, const char* text, int x, int y, uint32_t color) {
    auto* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}
void Surface(lv_obj_t* obj, uint32_t color, int radius) {
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}
lv_obj_t* Card(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color, int radius) {
    auto* card = lv_obj_create(parent);
    Surface(card, color, radius);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    return card;
}
lv_obj_t* Button(lv_obj_t* parent, const char* text, int action, int x, int y,
                 int width, int height, uint32_t color = 0x2B4350, uint32_t ink = kText) {
    lv_obj_t* button = lv_button_create(parent);
    Surface(button, color, 13);
    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_set_style_opa(button, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(button, lv_color_hex(ink), LV_PART_MAIN);
    lv_obj_add_event_cb(button, Click, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(action)));
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}
} // namespace

void Claw4TimerPreviewSetupUI(lv_obj_t* parent) {
    if (!lvgl_port_lock(1000)) return;
    entry = Button(parent, "Timer / Alarm", 7, 18, 92, 172, 50, kCard);
    entry_label = lv_obj_get_child(entry, 0);
    panel = Card(parent, 0, 0, 620, 496, kSurface, 26);
    lv_obj_center(panel);
    auto* title = Label(panel, "Timer / Alarm", 24, 19, kText);
    lv_obj_set_style_text_font(title, &font_noto_sans_basic_30_4, LV_PART_MAIN);
    close_panel = Button(panel, "Close", 8, 503, 18, 93, 44);

    auto* focus_card = Card(panel, 20, 82, 580, 238, kCard, 19);
    Label(focus_card, "Focus timer", 20, 18, kMint);
    focus_state = Label(focus_card, "Ready", 0, 0, kMint);
    lv_obj_align(focus_state, LV_ALIGN_TOP_RIGHT, -22, 18);
    focus_label = Label(focus_card, "00:00", 20, 47, kText);
    lv_obj_set_style_text_font(focus_label, &font_noto_sans_basic_30_4, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(focus_label, 448, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(focus_label, 448, LV_PART_MAIN);
    focus_progress = lv_bar_create(focus_card);
    lv_obj_set_pos(focus_progress, 20, 126);
    lv_obj_set_size(focus_progress, 540, 9);
    lv_bar_set_range(focus_progress, 0, 1000);
    lv_obj_set_style_bg_color(focus_progress, lv_color_hex(0x355260), LV_PART_MAIN);
    lv_obj_set_style_bg_color(focus_progress, lv_color_hex(kMint), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(focus_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(focus_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    focus_presets[0] = Button(focus_card, "1 min", kOneMinuteFocus, 20, 159, 86, 54);
    focus_presets[1] = Button(focus_card, "5 min", 10, 116, 159, 86, 54);
    focus_presets[2] = Button(focus_card, "25 min", 0, 212, 159, 97, 54, kMint, kSurface);
    focus_pause = Button(focus_card, "Pause", kToggleFocusPause, 335, 159, 110, 54);
    focus_pause_label = lv_obj_get_child(focus_pause, 0);
    focus_cancel = Button(focus_card, "Cancel", 3, 455, 159, 105, 54);

    auto* alarm_card = Card(panel, 20, 338, 580, 132, kCard, 19);
    Label(alarm_card, "One-time alarm", 20, 20, kAmber);
    alarm_label = Label(alarm_card, "No alarm set", 20, 53, kText);
    alarm_set = Button(alarm_card, "Alarm +1m", 9, 299, 39, 125, 54, kAmber, kSurface);
    alarm_cancel = Button(alarm_card, "Cancel alarm", 5, 435, 39, 125, 54);
    stop_alarm = Button(panel, "Stop Alarm", kAcknowledgeRinging, 20, 490, 580, 55, kRed);
    lv_obj_add_flag(stop_alarm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
    ESP_LOGI("Claw4Timer", "PREVIEW_UI_READY");
}
bool Claw4TimerPreviewIsRinging() { return ringing.load(); }
void Claw4TimerPreviewStart() {
    { std::lock_guard<std::mutex> lock(state_mutex); storage_ok = reminders.load(); alarm_due = AlarmDue(); }
    McpServer::GetInstance().AddTool("self.claw4.timer_preview", "Local focus timer and one-shot China-time alarm",
        PropertyList({Property("action", kPropertyTypeInteger, 0, 6),
                      Property("minutes", kPropertyTypeInteger, 25, 1, 120),
                      Property("hour", kPropertyTypeInteger, 0, 0, 23),
                      Property("minute", kPropertyTypeInteger, 0, 0, 59),
                      Property("day_offset", kPropertyTypeInteger, 0, 0, 1)}),
        [](const PropertyList& p) -> ToolResult {
            int action = p["action"].value<int>();
            const char* status = Action(action, p["minutes"].value<int>(), p["hour"].value<int>(),
                                        p["minute"].value<int>(), p["day_offset"].value<int>());
            cJSON* result = cJSON_CreateObject();
            if (!result) return std::unexpected("timer preview result allocation failed");
            cJSON_AddStringToObject(result, "status", status);
            { std::lock_guard<std::mutex> lock(state_mutex);
              cJSON_AddNumberToObject(result, "remaining_seconds", (Remaining() + 999) / 1000);
              if (alarm_due) { int64_t local = (alarm_due + kChina) / 60000;
                cJSON_AddNumberToObject(result, "alarm_hour", (local / 60) % 24);
                cJSON_AddNumberToObject(result, "alarm_minute", local % 60); }
            }
            return result;
        });
    xTaskCreate(TickTask, "claw4_timer", 4096, nullptr, 2, nullptr);
}
void Claw4TimerPreviewAcceptTime(int64_t epoch_ms) {
    std::lock_guard<std::mutex> lock(state_mutex);
    const auto result = authority.acceptSync(epoch_ms, "lan_ota", 1000);
    ESP_LOGI("Claw4Timer", "TIME_ANCHOR outcome=%d", static_cast<int>(result.outcome));
}
#ifdef CLAW4_LEARNING_PREVIEW
std::optional<int64_t> Claw4TimerPreviewTrustedTime() {
    std::lock_guard<std::mutex> lock(state_mutex);
    const auto status = authority.status();
    return status.calendar_allowed ? status.epoch_ms : std::nullopt;
}
#endif
#endif
