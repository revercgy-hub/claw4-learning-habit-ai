#pragma once
#include "audio/audio_codec.h"
#include <functional>
#include <mutex>
#include <cstdint>
#include <atomic>

// Only the electrical codec adapter. XiaoZhi owns capture, AFE and voice state.
class Claw4Audio final : public AudioCodec {
public:
    explicit Claw4Audio(std::function<void(bool)> amplifier);
    void EnableInput(bool enable) override;
    void EnableOutput(bool enable) override;
    bool InputData(std::vector<int16_t>& data) override;
protected:
    int Read(int16_t* data, int samples) override;
    int Write(const int16_t* data, int samples) override;
private:
    std::function<void(bool)> amplifier_;
    std::mutex input_mutex_;
    std::mutex output_mutex_;
#if CONFIG_CLAW4_M0_DIAGNOSTICS
    uint64_t energy_[2]{};
    uint32_t peak_[2]{};
    uint32_t clipped_[2]{};
    uint32_t raw_peak_[2]{};
    uint32_t samples_[2]{};
    uint32_t read_failures_ = 0;
    uint32_t tx_overlap_samples_[2]{};
    uint32_t tx_overlap_peak_[2]{};
    std::atomic<bool> tx_nonzero_active_{false};
    std::atomic<uint32_t> tx_frames_{0};
    int64_t last_stats_us_ = 0;
    void ReportInputStats();
#endif
};
