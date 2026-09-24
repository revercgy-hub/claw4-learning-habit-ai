#include "sdkconfig.h"
#if CONFIG_CLAW4_M0_DIAGNOSTICS
#include "application.h"
#include "board.h"
#include "claw4_audio.h"
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

void Claw4StartSdCardDiagnostic();

// Bounded local hardware harness, never starts the application protocol/OTA loop.
// M1 uses upstream main.cc unchanged by disabling this build option.
static std::atomic<bool> record_requested{false};
static std::atomic<unsigned> taps{0};
static std::atomic<unsigned> wake_events{0};
static std::atomic<unsigned> vad_events{0};
static std::atomic<unsigned> probe_sequence{0};
static std::atomic<unsigned> active_probe_id{0};
static std::atomic<unsigned> playback_vad_onsets{0};
static std::atomic<unsigned> playback_wake_detections{0};
static std::atomic<unsigned> probe_phase{0}; // 0=idle, 1=recording, 2=playback
static std::atomic<bool> rearm_requested{false};
static std::atomic<bool> sd_diagnostic_requested{false};

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
    auto* codec = static_cast<Claw4Audio*>(board.GetAudioCodec());
    audio.Initialize(codec);
    audio.Start();
    const bool assets = Assets::GetInstance().Apply();
    ESP_LOGI("V6M0", "Assets applied=%d", assets);
    AudioServiceCallbacks callbacks;
    callbacks.on_wake_word_detected = [](const std::string&) {
        const auto count = wake_events.fetch_add(1) + 1;
        const unsigned phase = probe_phase.load();
        const unsigned probe = phase == 0 ? 0 : active_probe_id.load();
        if (phase == 2) playback_wake_detections.fetch_add(1);
        ESP_LOGI("V6M0", "WAKE_DETECTED (local only) probe=%u phase=%u count=%u",
                 probe, phase, count);
        rearm_requested.store(true);
    };
    callbacks.on_vad_change = [](bool speaking) {
        const unsigned phase = probe_phase.load();
        const unsigned probe = phase == 0 ? 0 : active_probe_id.load();
        unsigned playback_onsets = playback_vad_onsets.load();
        if (speaking) {
            vad_events.fetch_add(1);
            if (phase == 2) playback_onsets = playback_vad_onsets.fetch_add(1) + 1;
        }
        ESP_LOGI("V6M0", "VAD probe=%u phase=%u speaking=%d count=%u playback_onsets=%u",
                 probe, phase, speaking, vad_events.load(), playback_onsets);
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
        if (event == NetworkEvent::Scanning) {
            // ESP-Hosted/C5 must own SDMMC Slot 1 before the Slot 0 card probe.
            sd_diagnostic_requested.store(true);
        }
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
    bool sd_diagnostic_started = false;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        if (!sd_diagnostic_started && sd_diagnostic_requested.exchange(false)) {
            sd_diagnostic_started = true;
            Claw4StartSdCardDiagnostic();
        }
        if (record_requested.exchange(false) && test == LocalTest::Idle) {
            const unsigned probe = probe_sequence.fetch_add(1) + 1;
            active_probe_id.store(probe);
            playback_vad_onsets.store(0);
            playback_wake_detections.store(0);
            probe_phase.store(1);
            ESP_LOGI("V6M0", "TOUCH count=%u; probe=%u phase=recording begin", taps.load(), probe);
            audio.EnableWakeWordDetection(false);
            rearm_after = 0;
            rearm_requested.store(false);
            // Start before recording: EnableVoiceProcessing resets decoder queues.
            // Keeping this input consumer alive permits RX during local playback.
            audio.EnableVoiceProcessing(true);
            audio.EnableAudioTesting(true);
            test = LocalTest::Recording;
            deadline = now + 3000000;
        }
        if (test == LocalTest::Recording && now >= deadline) {
            codec->BeginReferenceSession();
            probe_phase.store(2);
            audio.EnableWakeWordDetection(true);
            audio.EnableAudioTesting(false);
            ESP_LOGI("V6M0", "Audio testing playback queued probe=%u", active_probe_id.load());
            ESP_LOGI("V6M0", "LOCAL_REFERENCE_PROBE_BEGIN id=%u phase=playback wake_enabled=1; RX active; no upload",
                     active_probe_id.load());
            test = LocalTest::Playback;
            deadline = now + 10000000;
        }
        if (test == LocalTest::Playback && (audio.IsPlaybackIdle() || now >= deadline)) {
            const bool drained = audio.IsPlaybackIdle();
            if (!drained) audio.ResetDecoder();
            codec->EndReferenceSession();
            audio.EnableVoiceProcessing(false);
            probe_phase.store(0);
            ESP_LOGI("V6M0", "LOCAL_REFERENCE_PROBE_END id=%u drained=%d playback_vad_onsets=%u playback_wake_detections=%u",
                     active_probe_id.load(), drained, playback_vad_onsets.load(),
                     playback_wake_detections.load());
            rearm_requested.store(false);
            audio.EnableWakeWordDetection(true);
            test = LocalTest::Idle;
        }
        // AFE output is local diagnostic data, never handed to a protocol.
        for (unsigned discarded = 0; discarded < 8; ++discarded) {
            if (!audio.PopPacketFromSendQueue()) break;
        }
        if (test == LocalTest::Idle && rearm_requested.exchange(false)) rearm_after = now + 1000000;
        if (test == LocalTest::Idle && rearm_after && now >= rearm_after) {
            audio.EnableWakeWordDetection(true);
            rearm_after = 0;
            ESP_LOGI("V6M0", "WAKE_REARM armed=%d", audio.IsWakeWordRunning());
        }
        if (now >= next_health) {
            next_health = now + 10000000;
            ESP_LOGI("V6M0", "HEALTH free=%u psram=%u wake=%d taps=%u wakes=%u vads=%u local_playback_active=%d",
                     unsigned(esp_get_free_heap_size()),
                     unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                     audio.IsWakeWordRunning(), taps.load(), wake_events.load(), vad_events.load(),
                     test == LocalTest::Playback);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif
