#include "sdkconfig.h"
#if CONFIG_CLAW4_M0_DIAGNOSTICS
#include "application.h"
#include "board.h"
#include "display/display.h"
#include "assets.h"
#include "assets/lang_config.h"
#include <esp_event.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <nvs_flash.h>
#include <atomic>
#include <esp_timer.h>
#include <esp_app_desc.h>

// Bounded local hardware harness, never starts the application protocol/OTA loop.
// M1 uses upstream main.cc unchanged by disabling this build option.
static std::atomic<bool> record_requested{false};
static std::atomic<unsigned> taps{0};
static std::atomic<unsigned> wake_events{0};
static std::atomic<unsigned> vad_events{0};
static std::atomic<bool> rearm_requested{false};

extern "C" void app_main() {
    char app_sha[65]{};
    esp_app_get_elf_sha256(app_sha, sizeof(app_sha));
    ESP_LOGI("V6M0", "CANDIDATE_ELF_SHA256=%s", app_sha);
    ESP_ERROR_CHECK(nvs_flash_init()); // No automatic erase on migration failure.
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    auto& board = Board::GetInstance();
    auto* display = board.GetDisplay();
    display->SetupUI();
    display->SetStatus("Claw4 V6 / M0");
    display->SetChatMessage("system", "M0 hardware test: tap to record 3s then play. No cloud connection.");
    auto& audio = Application::GetInstance().GetAudioService();
    audio.Initialize(board.GetAudioCodec());
    audio.Start();
    const bool assets = Assets::GetInstance().Apply();
    ESP_LOGI("V6M0", "Assets applied=%d", assets);
    AudioServiceCallbacks callbacks;
    callbacks.on_wake_word_detected = [](const std::string&) {
        const auto count = wake_events.fetch_add(1) + 1;
        ESP_LOGI("V6M0", "WAKE_DETECTED (local only) count=%u", count);
        rearm_requested.store(true);
    };
    callbacks.on_vad_change = [](bool speaking) {
        if (speaking) vad_events.fetch_add(1);
        ESP_LOGI("V6M0", "VAD speaking=%d count=%u", speaking, vad_events.load());
    };
    audio.SetCallbacks(callbacks);
    audio.EnableWakeWordDetection(true);
    if (lvgl_port_lock(1000)) {
        lv_obj_t* button = lv_button_create(lv_layer_top());
        lv_obj_set_size(button, 300, 70);
        lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -20);
        auto* label = lv_label_create(button);
        lv_label_set_text(label, "Record 3s / Playback");
        lv_obj_center(label);
        lv_obj_add_event_cb(button, [](lv_event_t*) {
            taps.fetch_add(1);
            record_requested.store(true);
        }, LV_EVENT_CLICKED, nullptr);
        lvgl_port_unlock();
    }
    board.SetNetworkEventCallback([](NetworkEvent event, const std::string&) {
        ESP_LOGI("V6M0", "NETWORK_EVENT=%d", int(event)); // No SSIDs/credentials in our diagnostic log.
    });
    board.StartNetwork();
    ESP_LOGI("V6M0", "BOOT_READY IDF=%s; no protocol or OTA started", esp_get_idf_version());
    // Wake-only mode does not call HandleVoiceResult in the pinned AFE engine.
    // A missing VAD callback here is NOT evidence of silence/low input level.
    ESP_LOGI("V6M0", "VAD_OBSERVABLE=0 mode=wake_only; use INPUT statistics");
    enum class LocalTest { Idle, Recording, Playback };
    LocalTest test = LocalTest::Idle;
    int64_t deadline = 0;
    int64_t next_health = esp_timer_get_time() + 10000000;
    int64_t rearm_after = 0;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        if (record_requested.exchange(false) && test == LocalTest::Idle) {
            ESP_LOGI("V6M0", "TOUCH count=%u; audio record begin", taps.load());
            audio.EnableWakeWordDetection(false);
            audio.EnableAudioTesting(true);
            test = LocalTest::Recording;
            deadline = now + 3000000;
        }
        if (test == LocalTest::Recording && now >= deadline) {
            audio.EnableAudioTesting(false);
            ESP_LOGI("V6M0", "Audio testing playback queued");
            test = LocalTest::Playback;
            deadline = now + 3500000;
        }
        if (test == LocalTest::Playback && now >= deadline) {
            rearm_requested.store(false);
            audio.EnableWakeWordDetection(true);
            test = LocalTest::Idle;
        }
        if (test == LocalTest::Idle && rearm_requested.exchange(false)) rearm_after = now + 1000000;
        if (test == LocalTest::Idle && rearm_after && now >= rearm_after) {
            audio.EnableWakeWordDetection(true);
            rearm_after = 0;
            ESP_LOGI("V6M0", "WAKE_REARM armed=%d", audio.IsWakeWordRunning());
        }
        if (now >= next_health) {
            next_health = now + 10000000;
            ESP_LOGI("V6M0", "HEALTH free=%u psram=%u wake=%d taps=%u wakes=%u vads=%u vad_observable=0",
                     unsigned(esp_get_free_heap_size()),
                     unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                     audio.IsWakeWordRunning(), taps.load(), wake_events.load(), vad_events.load());
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif
