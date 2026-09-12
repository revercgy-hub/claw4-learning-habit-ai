#include "metalio_voice_session.h"

#include <esp_log.h>

#include "application.h"

namespace claw4 {
namespace metalio {

namespace {
constexpr const char* kTag = "LearningVoice";
}  // namespace

bool MetalioVoiceSessionPort::beginSession() {
  // 让唤醒词与录音通道归属语音 UI 会话，否则学习屏里说唤醒词不会响应。
  Application::GetInstance().SetVoiceUiDesired(true);
  ESP_LOGI(kTag, "session acquired (wake word armed for learning screen)");
  return true;
}

void MetalioVoiceSessionPort::endSession() {
  Application::GetInstance().SetVoiceUiDesired(false);
  ESP_LOGI(kTag, "session released");
}

void MetalioVoiceSessionPort::abortCloudReply() {
  // 停止当前 TTS/LLM 播报，避免学习命令被云端闲聊回复打断（或反之）。
  Application::GetInstance().AbortSpeaking(kAbortReasonNone);
  ESP_LOGI(kTag, "cloud reply aborted (learning command consumed)");
}

}  // namespace metalio
}  // namespace claw4
