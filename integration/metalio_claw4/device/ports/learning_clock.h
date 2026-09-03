// claw4/integration/metalio_claw4/device/ports/learning_clock.h
// WB-LEARNING-V4-L1 — device ClockPort adapter over esp_timer.
// Device-only TU (includes ESP-IDF headers); the codec/core stay host-safe.
#pragma once

#include "ports/clock_port.h"

namespace claw4 {
namespace metalio {

// monotonicMs: esp_timer microseconds / 1000 (never steps backwards).
// epochSeconds: boot-relative seconds until SNTP is wired in L2
//   (ARCHITECTURE.md: unsynced device timestamps are tagged Local and the
//   backend reconciles them; TimeSynced=false until then).
// isTimeSynced: false (no SNTP in L1 scope).
class EspTimerClock final : public claw4::ports::ClockPort {
 public:
  int64_t epochSeconds() override;
  int64_t monotonicMs() override;
  bool isTimeSynced() override;
};

}  // namespace metalio
}  // namespace claw4
