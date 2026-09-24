#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace claw4 {
// Normalize a signed full-width I2S slot to PCM16. Divide with floor semantics
// so negative values behave like an arithmetic shift on every host compiler.
constexpr int16_t DecodePcm16(int32_t slot) {
    const int64_t value = slot;
    return static_cast<int16_t>(value >= 0 ? value / 65536 : -((-value + 65535) / 65536));
}

// This is a headroom indicator on normalized PCM16, not proof of ADC or
// acoustic clipping. Eight or fewer PCM16 counts of positive headroom is near
// full scale; the negative endpoint is handled symmetrically.
constexpr uint32_t kPcm16NearFullScaleThreshold = 32760;
constexpr bool IsNearFullScalePcm16(int16_t sample) {
    const int32_t value = sample;
    const uint32_t magnitude = static_cast<uint32_t>(value < 0 ? -value : value);
    return magnitude >= kPcm16NearFullScaleThreshold;
}

// Bounded software TX reference. A token rejects I2S writes that complete after
// a playback session ended. This preserves sample order, but does not establish
// the physical TX-to-RX/acoustic delay; that requires device measurements.
class PlaybackReferenceDelay {
public:
    static constexpr std::size_t kPendingCapacity = 8192;
    static constexpr std::size_t kMaxDelaySamples = 2048;

    explicit constexpr PlaybackReferenceDelay(std::size_t delay_samples)
        : delay_samples_(delay_samples > kMaxDelaySamples ? kMaxDelaySamples : delay_samples) {}

    struct Stats {
        uint32_t overflow = 0;
        uint32_t underflow = 0;
        uint32_t stale_writes = 0;
        uint32_t discarded = 0;
    };

    uint64_t Begin() {
        End();
        active_ = true;
        return generation_;
    }

    void End() {
        Add(stats_.discarded, pending_size_ + delayed_pending_);
        pending_head_ = pending_tail_ = pending_size_ = delay_pos_ = 0;
        delay_.fill(0);
        delay_occupied_.fill(false);
        delayed_pending_ = 0;
        active_ = false;
        ++generation_;
        if (generation_ == 0) ++generation_;
    }

    bool Push(uint64_t token, int16_t sample) {
        if (!active_ || token != generation_) {
            Add(stats_.stale_writes, 1);
            return false;
        }
        if (pending_size_ == pending_.size()) {
            Add(stats_.overflow, 1);
            // A lost sample destroys the reference time base. Silence this
            // session until the owner explicitly begins another one.
            End();
            return false;
        }
        pending_[pending_tail_] = sample;
        pending_tail_ = (pending_tail_ + 1) % pending_.size();
        ++pending_size_;
        return true;
    }

    int16_t Next() {
        if (!active_) return 0;
        int16_t current = 0;
        bool current_valid = false;
        if (pending_size_ != 0) {
            current = pending_[pending_head_];
            pending_head_ = (pending_head_ + 1) % pending_.size();
            --pending_size_;
            current_valid = true;
        }
        if (delay_samples_ == 0) {
            if (!current_valid) Add(stats_.underflow, 1);
            return current;
        }
        const int16_t delayed = delay_[delay_pos_];
        const bool delayed_valid = delay_occupied_[delay_pos_];
        if (delayed_valid) --delayed_pending_;
        delay_[delay_pos_] = current;
        delay_occupied_[delay_pos_] = current_valid;
        if (current_valid) ++delayed_pending_;
        delay_pos_ = (delay_pos_ + 1) % delay_samples_;
        if (!delayed_valid) Add(stats_.underflow, 1);
        return delayed;
    }

    std::size_t pending_size() const { return pending_size_; }
    bool active() const { return active_; }
    uint64_t token() const { return active_ ? generation_ : 0; }
    Stats TakeStats() {
        const Stats result = stats_;
        stats_ = {};
        return result;
    }

private:
    static void Add(uint32_t& counter, std::size_t amount) {
        const uint32_t room = UINT32_MAX - counter;
        counter += static_cast<uint32_t>(amount > room ? room : amount);
    }
    std::array<int16_t, kPendingCapacity> pending_{};
    std::array<int16_t, kMaxDelaySamples> delay_{};
    std::array<bool, kMaxDelaySamples> delay_occupied_{};
    std::size_t delay_samples_;
    std::size_t pending_head_ = 0;
    std::size_t pending_tail_ = 0;
    std::size_t pending_size_ = 0;
    std::size_t delay_pos_ = 0;
    std::size_t delayed_pending_ = 0;
    uint64_t generation_ = 0;
    bool active_ = false;
    Stats stats_{};
};

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
