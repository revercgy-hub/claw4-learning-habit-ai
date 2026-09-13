// claw4/integration/metalio_claw4/device/app/learning_runtime.h
// WB-LEARNING-V4-L1 — device-side LearningApp singleton container.
// Owns the NVS outbox storage + esp_timer clock and the LearningApp glue;
// an explicitly unprovisioned first boot may seed the demo today snapshot.
// Device-only TU (includes the NVS/clock adapters).
#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sync/learning_backend_session.h"
#include "metalio_claw4/device/ports/learning_clock.h"
#include "metalio_claw4/device/ports/metalio_voice_session.h"
#include "metalio_claw4/device/ports/nvs_outbox_storage.h"
#include "metalio_claw4/host_glue/learning_app.h"

namespace claw4 {
namespace metalio {

class LearningRuntime {
 public:
  static LearningRuntime& Instance();

  // Idempotent; called lazily from the Learning screen. Constructs the app
  // over NVS storage + esp_timer clock, wires device identity and persisted
  // event/session id counters, then seeds the demo today snapshot exactly
  // once (only when provisioning is absent and no committed state exists).
  void Init();

  LearningApp& app() { return *app_; }
  bool inited() const { return inited_; }
  bool bootReady() const { return inited_ && app_ != nullptr && app_->running(); }
  // Screen callbacks and the backend worker must hold this lock while
  // touching the shared LearningApp/coordinator state.
  std::recursive_mutex& stateMutex() { return state_mutex_; }
  claw4::ports::ClockPort& clock() { return clock_; }

  // L4 (P25 Voice STT)：语音会话所有权。
  // 学习屏 LOAD 时 beginSession()、UNLOAD 时 endSession()，让唤醒词与麦克风
  // 归属语音 UI 会话（否则学习屏里说唤醒词不会响应）。实现在
  // ports/metalio_voice_session.{h,cpp}，仅映射到 Application::SetVoiceUiDesired。
  claw4::ports::VoiceSessionPort& voiceSession() { return voice_session_; }

  // Persisted monotonic id sources (NVS-backed, survive reboot) so a fresh
  // boot can never collide with pre-reboot pending event/session ids.
  std::string NextEventId();
  std::string NextSessionId();

  void StartBackendWorker();

  // Configure the L2/L3 backend session after provisioning supplies an
  // endpoint and signer. The signer is injected so no credential is embedded
  // in the firmware or logged by the learning app. RunOnlineCycle() must be
  // called from a worker/task context; it never blocks the LVGL callback.
  bool ConfigureBackend(
      std::string base_url, std::string device_id, std::string child_id,
      claw4::sync::LearningBackendSession::Signer signer);
  // Loads endpoint/identity/secret from the dedicated provisioning namespace
  // and creates the injected HMAC signer. Returns false when unprovisioned.
  bool ConfigureProvisionedBackend();
  bool RunOnlineCycle();
  claw4::sync::BackendSessionDiagnostics BackendDiagnostics() const;

 private:
  LearningRuntime() = default;

  NvsOutboxStorage storage_;
  EspTimerClock clock_;
  MetalioVoiceSessionPort voice_session_;
  std::unique_ptr<LearningApp> app_;
  std::unique_ptr<claw4::sync::LearningBackendSession> backend_;
  TaskHandle_t backend_task_ = nullptr;
  mutable std::recursive_mutex state_mutex_;
  std::string device_id_ = "dev-claw4-l1";
  std::string child_id_ = "child-1";
  bool inited_ = false;
};

}  // namespace metalio
}  // namespace claw4
