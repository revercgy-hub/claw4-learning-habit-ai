// claw4/firmware/main/ports/stt_port.h
// SttPort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// MVP boundary only: enable/disable the recognizer and receive text results.
// No ASR engine is implemented in MVP; the P17 adapter wires the real engine
// (and its wake-word exclusivity) behind this port. Portable C++17.
#pragma once

#include <functional>
#include <string>

namespace claw4 {
namespace ports {

class SttPort {
 public:
  virtual ~SttPort() = default;
  using ResultCallback = std::function<void(const std::string& text)>;

  virtual void setEnabled(bool enabled) = 0;
  virtual void setResultCallback(ResultCallback callback) = 0;
};

}  // namespace ports
}  // namespace claw4
