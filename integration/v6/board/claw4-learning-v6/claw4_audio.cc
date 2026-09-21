#include "claw4_audio.h"
#include "config.h"
#include <algorithm>
#include <array>
#include <utility>
#include <esp_log.h>

Claw4Audio::Claw4Audio(std::function<void(bool)> amplifier)
    : amplifier_(std::move(amplifier)) {
    duplex_ = true;
    input_sample_rate_ = output_sample_rate_ = 16000;
    input_reference_ = true;
    input_channels_ = 2; // Metalio interleaved microphone / playback reference.
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
        if (err != ESP_OK || bytes != count * sizeof(int32_t)) return 0;
        for (int i = 0; i < count; ++i) {
            // Preserve the source board's ADC alignment/gain, verify both channels on hardware.
            dest[total + i] = static_cast<int16_t>(std::clamp<int32_t>(buffer[i] >> 12, -32768, 32767));
        }
        total += count;
    }
    return total;
}

int Claw4Audio::Write(const int16_t* data, int samples) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (!output_enabled_ || samples <= 0) return 0;
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
        const auto err = i2s_channel_write(tx_handle_, buffer.data(), count * 2 * sizeof(int32_t),
                                           &bytes, 200);
        total += static_cast<int>(bytes / (2 * sizeof(int32_t)));
        if (err != ESP_OK || bytes != count * 2 * sizeof(int32_t)) {
            ESP_LOGW("Claw4Audio", "I2S output incomplete: %s", esp_err_to_name(err));
            return total;
        }
    }
    return total;
}
