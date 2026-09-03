// claw4/integration/metalio_claw4/device/ports/learning_clock.cpp
// WB-LEARNING-V4-L1 — esp_timer-backed ClockPort.
#include "metalio_claw4/device/ports/learning_clock.h"

#include <cstdint>

#include "esp_timer.h"

namespace claw4 {
namespace metalio {

int64_t EspTimerClock::epochSeconds() {
  // Unsynchronized: seconds since boot. L2 SNTP will switch this to the RTC.
  return esp_timer_get_time() / 1000000LL;
}

int64_t EspTimerClock::monotonicMs() {
  return esp_timer_get_time() / 1000LL;
}

bool EspTimerClock::isTimeSynced() { return false; }

}  // namespace metalio
}  // namespace claw4
