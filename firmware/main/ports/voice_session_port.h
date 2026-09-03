// claw4/firmware/main/ports/voice_session_port.h
// VoiceSessionPort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// MVP boundary for voice-session ownership. The P17 adapter maps begin/end to
// Metalio's SetVoiceUiDesired semantics (voice UI / mic exclusivity).
// Portable C++17.
#pragma once

namespace claw4 {
namespace ports {

class VoiceSessionPort {
 public:
  virtual ~VoiceSessionPort() = default;
  // Acquire the voice session (e.g. before the child speaks a completion
  // confirmation). Returns false when the session cannot be acquired.
  virtual bool beginSession() = 0;
  // Release the voice session.
  virtual void endSession() = 0;
};

}  // namespace ports
}  // namespace claw4
