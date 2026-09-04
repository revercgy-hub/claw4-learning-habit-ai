// claw4/integration/metalio_claw4/device/app/learning_runtime.h
// WB-LEARNING-V4-L1 — device-side LearningApp singleton container.
// Owns the NVS outbox storage + esp_timer clock and the LearningApp glue;
// first-boot seeds the demo today snapshot (guarded by hasState()).
// Device-only TU (includes the NVS/clock adapters).
#pragma once

#include <memory>
#include <string>

#include "metalio_claw4/device/ports/learning_clock.h"
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
  // once (when no committed outbox state exists yet).
  void Init();

  LearningApp& app() { return *app_; }
  bool inited() const { return inited_; }
  claw4::ports::ClockPort& clock() { return clock_; }

  // Persisted monotonic id sources (NVS-backed, survive reboot) so a fresh
  // boot can never collide with pre-reboot pending event/session ids.
  std::string NextEventId();
  std::string NextSessionId();

 private:
  LearningRuntime() = default;
  int64_t NextNvsCounter(const char* key);

  NvsOutboxStorage storage_;
  EspTimerClock clock_;
  std::unique_ptr<LearningApp> app_;
  bool inited_ = false;
};

}  // namespace metalio
}  // namespace claw4
