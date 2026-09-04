// claw4/integration/metalio_claw4/device/app/learning_runtime.cpp
// WB-LEARNING-V4-L1 — device LearningApp runtime (see header).
#include "metalio_claw4/device/app/learning_runtime.h"

#include <cstdio>

#include "esp_log.h"
#include "nvs.h"

#include "metalio_claw4/device/core/demo_seed.h"
#include "metalio_claw4/device/core/outbox_codec.h"

namespace claw4 {
namespace metalio {
namespace {

constexpr const char* TAG = "LearningRt";
constexpr const char* kEvCounter = "evseq";
constexpr const char* kSessCounter = "sessseq";

// MVP identity (L2 provisioning replaces with the enrolled child/device ids).
constexpr const char* kDeviceId = "dev-claw4-l1";
constexpr const char* kChildId = "child-1";

std::string Hex(int64_t v) {
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%llx",
                static_cast<unsigned long long>(v));
  return buf;
}

}  // namespace

LearningRuntime& LearningRuntime::Instance() {
  static LearningRuntime rt;
  return rt;
}

int64_t LearningRuntime::NextNvsCounter(const char* key) {
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READWRITE, &h) != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed for counter %s", key);
    return 0;
  }
  int64_t v = 0;
  nvs_get_i64(h, key, &v);
  ++v;
  nvs_set_i64(h, key, v);
  nvs_commit(h);
  nvs_close(h);
  return v;
}

std::string LearningRuntime::NextEventId() {
  return "ev-" + Hex(NextNvsCounter(kEvCounter));
}

std::string LearningRuntime::NextSessionId() {
  return "sess-" + Hex(NextNvsCounter(kSessCounter));
}

void LearningRuntime::Init() {
  if (inited_) return;
  app_ = std::make_unique<LearningApp>(storage_, clock_);
  app_->setIdentity(claw4::domain::DeviceId{kDeviceId},
                    claw4::domain::ChildId{kChildId});
  app_->setEventIdFactory([this] {
    return claw4::domain::EventId{NextEventId()};
  });
  app_->setSessionIdFactory([this] {
    return claw4::domain::SessionId{NextSessionId()};
  });
  if (!storage_.hasState()) {
    const bool ok = app_->applyTodaySnapshot(DemoTodaySnapshot());
    ESP_LOGI(TAG, "first boot: demo today snapshot seeded=%d", ok ? 1 : 0);
  } else {
    ESP_LOGI(TAG,
             "boot with committed state: tasks=%zu pending=%d active=%d",
             app_->state().tasks.size(), app_->pendingCount(),
             app_->state().active_session.has_value() ? 1 : 0);
  }
  app_->start();
  inited_ = true;
}

}  // namespace metalio
}  // namespace claw4
