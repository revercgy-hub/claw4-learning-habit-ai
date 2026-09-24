#pragma once
#include "audio/audio_codec.h"
#include "board_algorithms.h"
#include <functional>
#include <mutex>
#include <cstdint>
#include <cstddef>
#include <atomic>

// Only the electrical codec adapter. XiaoZhi owns capture, AFE and voice state.
class Claw4Audio final : public AudioCodec {
public:
    explicit Claw4Audio(std::function<void(bool)> amplifier);
    // M0 local probe boundary; XiaoZhi retains ownership of audio playback.
    void BeginReferenceSession();
    void EndReferenceSession();
    void EnableInput(bool enable) override;
    void EnableOutput(bool enable) override;
    bool InputData(std::vector<int16_t>& data) override;
protected:
    int Read(int16_t* data, int samples) override;
    int Write(const int16_t* data, int samples) override;
private:
    // Zero is an uncalibrated software ordering baseline, not acoustic alignment.
    static constexpr std::size_t kPlaybackReferenceDelayFrames = 0;
    std::function<void(bool)> amplifier_;
    std::mutex input_mutex_;
    std::mutex output_mutex_;
    std::mutex reference_mutex_;
    claw4::PlaybackReferenceDelay playback_reference_{
        kPlaybackReferenceDelayFrames};
#if CONFIG_CLAW4_M0_DIAGNOSTICS
    uint64_t energy_[2]{};
    uint32_t peak_[2]{};
    uint32_t near_full_scale_[2]{};
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
