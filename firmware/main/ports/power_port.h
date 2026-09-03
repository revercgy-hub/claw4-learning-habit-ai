// claw4/firmware/main/ports/power_port.h
// PowerPort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// MVP boundary: battery state + screen-awake requests. Portable C++17.
#pragma once

namespace claw4 {
namespace ports {

struct PowerInfo {
  int battery_percent = 0;
  bool charging = false;
};

class PowerPort {
 public:
  virtual ~PowerPort() = default;
  virtual PowerInfo info() = 0;
  // Keep the screen/device awake while a focus session is running.
  virtual void stayAwake(bool keep_awake) = 0;
};

}  // namespace ports
}  // namespace claw4
