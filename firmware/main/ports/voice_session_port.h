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
  // L4 (P25 Voice STT): 命中学习命令后立即中止正在进行的云端回复（LLM/TTS）。
  // 上游是 ASR→LLM 服务端流水线，识别文本到达设备时服务端可能已在生成回复，
  // 设备侧只能尽力掐断，避免孩子在学习页听到无关闲聊。
  // 提供默认空实现：主机侧与测试替身无需关心该行为。
  virtual void abortCloudReply() {}
};

}  // namespace ports
}  // namespace claw4
