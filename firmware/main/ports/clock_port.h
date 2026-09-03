// claw4/firmware/main/ports/clock_port.h
// ClockPort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// Portable C++17. No ESP-IDF / Metalio includes. The device adapter (P17)
// bridges this to the RTC / SNTP / esp_timer sources.
#pragma once

#include <cstdint>

namespace claw4 {
namespace ports {

class ClockPort {
 public:
  virtual ~ClockPort() = default;
  virtual int64_t epochSeconds() = 0;   // unix epoch seconds (RTC when synced)
  virtual int64_t monotonicMs() = 0;    // monotonic ms (never steps backwards)
  virtual bool isTimeSynced() = 0;      // SNTP sync state
};

}  // namespace ports
}  // namespace claw4
