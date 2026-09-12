#pragma once

#include "ports/voice_session_port.h"

namespace claw4 {
namespace metalio {

// Metalio (ESP32-P4 / xiaozhi) 侧语音会话实现（V4 §9 / WB-LEARNING-V4 P16）。
//
// 语义映射：
//   beginSession() -> Application::SetVoiceUiDesired(true)
//       把唤醒词与麦克风归属到语音 UI 会话。学习屏 LOAD 时调用。
//       上游 application.cc 注释明确：「唤醒词只属于语音 UI 会话」——
//       不调用本方法，学习屏里说唤醒词不会响应。
//   endSession()   -> Application::SetVoiceUiDesired(false)
//       软停语音通道并延后硬释放。学习屏 UNLOAD 时调用。
//
// 安全性：
//   - SetVoiceUiDesired 对相同取值幂等；
//   - 内部经 Application::Schedule 调度，不阻塞调用者（不要在 LVGL 回调里做重活）；
//   - 学习屏与聊天/数字人屏不会同时处于前台，因此不构成麦克风抢占。
class MetalioVoiceSessionPort : public claw4::ports::VoiceSessionPort {
 public:
  bool beginSession() override;
  void endSession() override;
  // 命中学习命令后立即掐断云端回复：映射到 Application::AbortSpeaking()。
  // 注意这是「尽力而为」——上游为 ASR→LLM 服务端流水线，设备收到 stt 文本时
  // 服务端可能已生成部分回复，仍可能有短暂残余语音。
  void abortCloudReply() override;
};

}  // namespace metalio
}  // namespace claw4
