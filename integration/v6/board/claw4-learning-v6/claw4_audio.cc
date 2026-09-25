#include "claw4_audio.h"
#include "config.h"
#include "board_algorithms.h"
#include <algorithm>
#include <array>
#include <utility>
#include <esp_log.h>
#include <esp_timer.h>
#include <cmath>

Claw4Audio::Claw4Audio(std::function<void(bool)> amplifier)
    : amplifier_(std::move(amplifier)) {
    duplex_ = true;
    input_sample_rate_ = output_sample_rate_ = 16000;
    input_reference_ = true;
    input_channels_ = 2; // Metalio interleaved microphone / reference slots.
    i2s_chan_config_t channels = I2S_CHANNEL_DEFAULT_CONFIG(XIAOZHI_I2S_PORT(0), I2S_ROLE_SLAVE);
    channels.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    channels.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    channels.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&channels, &tx_handle_, &rx_handle_));
    i2s_std_config_t config = {};
    config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
    config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_STEREO);
    config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    config.gpio_cfg.bclk = AUDIO_BCLK;
    config.gpio_cfg.ws = AUDIO_WS;
    config.gpio_cfg.dout = AUDIO_DOUT;
    config.gpio_cfg.din = AUDIO_DIN;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &config));
    ESP_LOGI("Claw4Audio", "I2S slave 16kHz stereo32; mic+reference; bounded IO");
    ESP_LOGI("Claw4Audio", "Software playback reference delay_frames=%u source=accepted_post_volume_TX alignment=uncalibrated",
             unsigned(kPlaybackReferenceDelayFrames));
}

void Claw4Audio::BeginReferenceSession() {
    std::lock_guard<std::mutex> lock(reference_mutex_);
    playback_reference_.Begin();
}

void Claw4Audio::EndReferenceSession() {
    std::lock_guard<std::mutex> lock(reference_mutex_);
    playback_reference_.End();
}

void Claw4Audio::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(input_mutex_);
    if (input_enabled_ == enable) return;
    ESP_ERROR_CHECK(enable ? i2s_channel_enable(rx_handle_) : i2s_channel_disable(rx_handle_));
    AudioCodec::EnableInput(enable);
}

void Claw4Audio::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (output_enabled_ == enable) return;
    if (!enable) amplifier_(false);
    ESP_ERROR_CHECK(enable ? i2s_channel_enable(tx_handle_) : i2s_channel_disable(tx_handle_));
    if (enable) BeginReferenceSession();
    else EndReferenceSession();
    if (enable) amplifier_(true);
    AudioCodec::EnableOutput(enable);
}

bool Claw4Audio::InputData(std::vector<int16_t>& data) {
    const int read = Read(data.data(), static_cast<int>(data.size()));
    if (read != static_cast<int>(data.size())) {
        std::fill(data.begin(), data.end(), 0);
        return false; // Never feed an uninitialized/partial frame to the AFE.
    }
    return true;
}

int Claw4Audio::Read(int16_t* dest, int samples) {
    std::lock_guard<std::mutex> lock(input_mutex_);
    if (!input_enabled_ || samples <= 0) return 0;
    std::array<int32_t, 256> buffer{};
    int total = 0;
    while (total < samples) {
        const int count = std::min(samples - total, static_cast<int>(buffer.size()));
        size_t bytes = 0;
        const auto err = i2s_channel_read(rx_handle_, buffer.data(), count * sizeof(int32_t),
                                          &bytes, 200);
        if (err != ESP_OK || bytes != count * sizeof(int32_t)) {
#if CONFIG_CLAW4_M0_DIAGNOSTICS
            ++read_failures_;
            ReportInputStats();
#endif
            return 0;
        }
        std::lock_guard<std::mutex> reference_lock(reference_mutex_);
        for (int i = 0; i < count; ++i) {
            // Candidate 05 measured full-width slots; >>12 added 24 dB before
            // saturation. Normalize first; any later gain must be explicit.
            dest[total + i] = claw4::DecodePcm16(buffer[i]);
            if ((total + i) % 2 == 1) {
                // Hardware R slot was silent on Candidate20. Feed the accepted
                // post-volume TX stream as an uncalibrated AFE reference.
                dest[total + i] = playback_reference_.Next();
            }
#if CONFIG_CLAW4_M0_DIAGNOSTICS
            const unsigned channel = (total + i) % 2;
            // ch=1 describes the AFE reference; raw_peak_ is the physical RX.
            const int64_t value = dest[total + i];
            const uint32_t magnitude = value < 0 ? -value : value;
            energy_[channel] += value * value;
            peak_[channel] = std::max(peak_[channel], magnitude);
            if (claw4::IsNearFullScalePcm16(dest[total + i])) {
                ++near_full_scale_[channel];
            }
            const int64_t raw = buffer[i];
            raw_peak_[channel] = std::max(raw_peak_[channel], uint32_t(raw < 0 ? -raw : raw));
            // Full-width normalization has no out-of-range conversion. Raw
            // near-full-scale peaks still need independent source analysis.
            if (tx_nonzero_active_.load(std::memory_order_relaxed)) {
                ++tx_overlap_samples_[channel];
                tx_overlap_peak_[channel] = std::max(tx_overlap_peak_[channel], magnitude);
            }
            ++samples_[channel];
#endif
        }
        total += count;
    }
    ReportReferenceStats();
#if CONFIG_CLAW4_M0_DIAGNOSTICS
    ReportInputStats();
#endif
    return total;
}

void Claw4Audio::ReportReferenceStats() {
    const int64_t now = esp_timer_get_time();
    if (now - last_reference_stats_us_ < 1000000) return;
    last_reference_stats_us_ = now;
    std::lock_guard<std::mutex> lock(reference_mutex_);
    const auto stats = playback_reference_.TakeStats();
    ESP_LOGI("Claw4Audio", "TX_REFERENCE source=accepted_software_post_volume delay_frames=%u alignment=uncalibrated active=%u overflow=%u underflow=%u stale_writes=%u discarded=%u pending=%u",
             unsigned(kPlaybackReferenceDelayFrames), unsigned(playback_reference_.active()),
             unsigned(stats.overflow), unsigned(stats.underflow),
             unsigned(stats.stale_writes), unsigned(stats.discarded),
             unsigned(playback_reference_.pending_size()));
}

#if CONFIG_CLAW4_M0_DIAGNOSTICS
void Claw4Audio::ReportInputStats() {
    const int64_t now = esp_timer_get_time();
    if (now - last_stats_us_ < 1000000) return;
    for (unsigned ch = 0; ch < 2; ++ch) {
        const unsigned rms = samples_[ch] ? unsigned(std::sqrt(double(energy_[ch]) / samples_[ch])) : 0;
        ESP_LOGI("Claw4Audio", "INPUT ch=%u n=%u rms=%u peak=%u near_full_scale_n=%u raw_peak=%u read_failures=%u tx_overlap_n=%u tx_overlap_peak=%u tx_frames=%u",
                 ch, unsigned(samples_[ch]), rms, unsigned(peak_[ch]), unsigned(near_full_scale_[ch]),
                 unsigned(raw_peak_[ch]), unsigned(read_failures_), unsigned(tx_overlap_samples_[ch]),
                 unsigned(tx_overlap_peak_[ch]), unsigned(tx_frames_.load()));
        energy_[ch] = peak_[ch] = near_full_scale_[ch] = raw_peak_[ch] = samples_[ch] = 0;
        tx_overlap_samples_[ch] = tx_overlap_peak_[ch] = 0;
    }
    read_failures_ = 0;
    last_stats_us_ = now;
}
#endif

int Claw4Audio::Write(const int16_t* data, int samples) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (!output_enabled_ || samples <= 0) return 0;
    uint64_t reference_token = 0;
    {
        std::lock_guard<std::mutex> reference_lock(reference_mutex_);
        // Duplex output may remain enabled between utterances. Recover after
        // a bounded queue overflow without taking over upstream playback.
        if (!playback_reference_.active()) playback_reference_.Begin();
        reference_token = playback_reference_.token();
    }
    std::array<int32_t, 512> buffer{};
    const int volume = std::clamp(output_volume_, 0, 100);
    const int64_t factor = int64_t(volume) * volume * 65536 / 10000;
    int total = 0;
    while (total < samples) {
        const int count = std::min(samples - total, 256);
        for (int i = 0; i < count; ++i) {
            const auto sample = static_cast<int32_t>(std::clamp<int64_t>(
                int64_t(data[total + i]) * factor, INT32_MIN, INT32_MAX));
            buffer[i * 2] = buffer[i * 2 + 1] = sample;
        }
        size_t bytes = 0;
#if CONFIG_CLAW4_M0_DIAGNOSTICS
        tx_nonzero_active_.store(std::any_of(buffer.begin(), buffer.begin() + count * 2,
                                            [](int32_t value) { return value != 0; }));
#endif
        const auto err = i2s_channel_write(tx_handle_, buffer.data(), count * 2 * sizeof(int32_t),
                                           &bytes, 200);
#if CONFIG_CLAW4_M0_DIAGNOSTICS
        tx_nonzero_active_.store(false);
        tx_frames_.fetch_add(bytes / (2 * sizeof(int32_t)));
#endif
        const int written_frames = static_cast<int>(bytes / (2 * sizeof(int32_t)));
        {
            std::lock_guard<std::mutex> reference_lock(reference_mutex_);
            for (int i = 0; i < written_frames; ++i) {
                playback_reference_.Push(reference_token, claw4::DecodePcm16(buffer[i * 2]));
            }
        }
        total += static_cast<int>(bytes / (2 * sizeof(int32_t)));
        if (err != ESP_OK || bytes != count * 2 * sizeof(int32_t)) {
            ESP_LOGW("Claw4Audio", "I2S output incomplete: %s", esp_err_to_name(err));
            return total;
        }
    }
    return total;
}
