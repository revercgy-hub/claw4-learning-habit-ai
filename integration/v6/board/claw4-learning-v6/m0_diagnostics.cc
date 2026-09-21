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

// Bounded local hardware harness, never starts the application protocol/OTA loop.
// M1 uses upstream main.cc unchanged by disabling this build option.
static std::atomic<bool> record_requested{false};
static std::atomic<unsigned> taps{0};

extern "C" void app_main() {
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
    callbacks.on_wake_word_detected = [](const std::string&) { ESP_LOGI("V6M0", "WAKE_DETECTED (local only)"); };
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
    unsigned ticks = 0;
    for (;;) {
        if (record_requested.exchange(false)) {
            ESP_LOGI("V6M0", "TOUCH count=%u; audio record begin", taps.load());
            audio.EnableWakeWordDetection(false);
            audio.EnableAudioTesting(true);
            vTaskDelay(pdMS_TO_TICKS(3000));
            audio.EnableAudioTesting(false);
            ESP_LOGI("V6M0", "Audio testing playback queued");
            vTaskDelay(pdMS_TO_TICKS(3500));
            audio.EnableWakeWordDetection(true);
        }
        if (++ticks % 10 == 0) {
            ESP_LOGI("V6M0", "HEALTH free=%u psram=%u wake=%d taps=%u",
                     unsigned(esp_get_free_heap_size()),
                     unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                     audio.IsWakeWordRunning(), taps.load());
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif
