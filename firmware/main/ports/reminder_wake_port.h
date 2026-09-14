// claw4/firmware/main/ports/reminder_wake_port.h
// WB-V53-NEXT-001 CP4 (B03): platform-neutral wake/tick boundary.
//
// The reminder core never references esp_sleep / LVGL / FreeRTOS. A device
// adapter implements this port on top of the platform timer (or a Light Sleep
// wake source in a later stage); the host fake drives it from a plain counter.
// Portable C++17.
#pragma once

#include <cstdint>

namespace claw4 {
namespace ports {

class ReminderWakePort {
 public:
  virtual ~ReminderWakePort() = default;
  // Current monotonic milliseconds (free-running, survives only a boot).
  virtual int64_t monotonicMs() = 0;
  // Requests that the platform wake the reminder evaluator no later than the
  // given monotonic deadline. Returns false when the platform cannot honour the
  // request (the core then falls back to its periodic tick).
  virtual bool scheduleWakeAt(int64_t monotonic_deadline_ms) = 0;
  virtual void cancelWake() = 0;
};

}  // namespace ports
}  // namespace claw4
