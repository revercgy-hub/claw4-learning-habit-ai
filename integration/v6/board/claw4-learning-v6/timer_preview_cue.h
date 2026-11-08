#pragma once
#include <cstdint>

namespace claw4 {
enum class CueDecision { None, Deferred, Queued, Expired };

// One cue per expiry, with a bounded wait for an undisturbed audio path.
class TimerPreviewCue {
public:
    void Arm(int64_t now_ms, int64_t wait_ms) {
        pending_ = true;
        queued_ = false;
        expired_ = false;
        deferred_reported_ = false;
        deadline_ms_ = now_ms + wait_ms;
    }
    void Cancel() { pending_ = false; queued_ = false; expired_ = false; deferred_reported_ = false; }
    bool pending() const { return pending_; }
    bool queued() const { return queued_; }
    bool expired() const { return expired_; }
    CueDecision Poll(int64_t now_ms, bool can_play) {
        if (!pending_) return CueDecision::None;
        if (now_ms >= deadline_ms_) { pending_ = false; expired_ = true; return CueDecision::Expired; }
        if (can_play) { pending_ = false; queued_ = true; return CueDecision::Queued; }
        if (!deferred_reported_) { deferred_reported_ = true; return CueDecision::Deferred; }
        return CueDecision::None;
    }
private:
    int64_t deadline_ms_ = 0;
    bool pending_ = false;
    bool queued_ = false;
    bool expired_ = false;
    bool deferred_reported_ = false;
};
} // namespace claw4
