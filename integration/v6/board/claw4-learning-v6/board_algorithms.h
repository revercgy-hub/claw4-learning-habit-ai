#pragma once
#include <cstdint>

namespace claw4 {
// Normalize a signed full-width I2S slot to PCM16. Divide with floor semantics
// so negative values behave like an arithmetic shift on every host compiler.
constexpr int16_t DecodePcm16(int32_t slot) {
    const int64_t value = slot;
    return static_cast<int16_t>(value >= 0 ? value / 65536 : -((-value + 65535) / 65536));
}

enum class ButtonEvent { None, ShortPress, LongPress };

// Active-low, edge-triggered short/long press recognition. An input already
// held during boot is ignored until a stable release, avoiding phantom events.
class ActiveLowButtonDebouncer {
public:
    explicit constexpr ActiveLowButtonDebouncer(uint64_t debounce_ms = 50,
                                                uint64_t long_press_ms = 1500)
        : debounce_ms_(debounce_ms), long_press_ms_(long_press_ms) {}

    void Begin(bool pressed, uint64_t now_ms) {
        initialized_ = true;
        stable_pressed_ = pressed;
        candidate_pressed_ = pressed;
        candidate_since_ms_ = now_ms;
        armed_ = !pressed;
    }

    ButtonEvent Sample(bool pressed, uint64_t now_ms) {
        if (!initialized_) {
            Begin(pressed, now_ms);
            return ButtonEvent::None;
        }
        if (pressed != candidate_pressed_) {
            candidate_pressed_ = pressed;
            candidate_since_ms_ = now_ms;
        }
        if (candidate_pressed_ != stable_pressed_ &&
            now_ms - candidate_since_ms_ >= debounce_ms_) {
            stable_pressed_ = candidate_pressed_;
            if (!armed_) {
                if (!stable_pressed_) armed_ = true;
                return ButtonEvent::None;
            }
            if (stable_pressed_) {
                press_active_ = true;
                long_fired_ = false;
                press_started_ms_ = now_ms;
            } else if (press_active_) {
                press_active_ = false;
                if (!long_fired_) return ButtonEvent::ShortPress;
                long_fired_ = false;
            }
        }
        if (stable_pressed_ && press_active_ && !long_fired_ &&
            now_ms - press_started_ms_ >= long_press_ms_) {
            long_fired_ = true;
            return ButtonEvent::LongPress;
        }
        return ButtonEvent::None;
    }

private:
    uint64_t debounce_ms_;
    uint64_t long_press_ms_;
    uint64_t candidate_since_ms_ = 0;
    uint64_t press_started_ms_ = 0;
    bool initialized_ = false;
    bool stable_pressed_ = false;
    bool candidate_pressed_ = false;
    bool armed_ = false;
    bool press_active_ = false;
    bool long_fired_ = false;
};

// Recovery policy is shared with host fault-injection tests. Hardware operations
// and the final diagnostic halt remain the board adapter's responsibility.
template<typename Operation, typename Recover, typename Wait>
auto RetryThree(Operation operation, Recover recover, Wait wait) {
    auto status = operation(1U);
    for (unsigned attempt = 2; status != 0 && attempt <= 3; ++attempt) {
        recover();
        wait(100U * (attempt - 1));
        status = operation(attempt);
    }
    return status;
}
}
