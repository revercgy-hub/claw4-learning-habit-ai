#pragma once
#include "audio/audio_codec.h"
#include <functional>
#include <mutex>

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
};
