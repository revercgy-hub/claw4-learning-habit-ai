#pragma once
#include <cstdint>

namespace claw4 {
// Ring in pairs with a longer gap. At most one clip is requested per poll,
// and a busy output delays the next clip without clearing the alarm.
class TimerPreviewCue {
public:
    void Arm(int64_t now_ms) {
        if (active_) return;
        active_ = true;
        second_in_pair_ = false;
        next_ms_ = now_ms;
    }
    void Acknowledge() { active_ = false; }
    bool active() const { return active_; }
    bool Poll(int64_t now_ms, bool can_play) {
        if (!active_ || !can_play || now_ms < next_ms_) return false;
        next_ms_ = now_ms + (second_in_pair_ ? 1500 : 500);
        second_in_pair_ = !second_in_pair_;
        return true;
    }
private:
    int64_t next_ms_ = 0;
    bool active_ = false;
    bool second_in_pair_ = false;
};
} // namespace claw4
