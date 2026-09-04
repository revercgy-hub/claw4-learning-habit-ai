// claw4/integration/metalio_claw4/device/app/learning_runtime.cpp
// WB-LEARNING-V4-L1 — device LearningApp runtime (see header).
#include "metalio_claw4/device/app/learning_runtime.h"

#include <memory>
#include <utility>

#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"

#include "metalio_claw4/device/core/demo_seed.h"
#include "metalio_claw4/device/core/outbox_codec.h"

namespace claw4 {
namespace metalio {
namespace {

constexpr const char* TAG = "LearningRt";

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

std::string LearningRuntime::NextEventId() {
  // Random 63-bit id (two 32-bit draws): no persisted counter is needed, so a
  // live event can never collide with a pre-reboot pending id and the outbox
  // duplicate guard never misfires after a reboot.
  const int64_t hi = static_cast<int64_t>(esp_random()) << 32;
  const int64_t lo = static_cast<int64_t>(esp_random());
  return "ev-" + Hex((hi | lo) & 0x7FFFFFFFFFFFFFFFLL);
}

std::string LearningRuntime::NextSessionId() {
  const int64_t hi = static_cast<int64_t>(esp_random()) << 32;
  const int64_t lo = static_cast<int64_t>(esp_random());
  return "sess-" + Hex((hi | lo) & 0x7FFFFFFFFFFFFFFFLL);
}

bool LearningRuntime::SelfTestPending() {
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READONLY, &h) != ESP_OK) return false;
  int32_t flag = 0;
  const esp_err_t rc = nvs_get_i32(h, "stest", &flag);
  nvs_close(h);
  return rc == ESP_ERR_NVS_NOT_FOUND || flag == 0;
}

void LearningRuntime::MarkSelfTestDone() {
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_i32(h, "stest", 1);
  nvs_commit(h);
  nvs_close(h);
}

bool LearningRuntime::ResetToSeed() {
  if (!NvsOutboxStorage::eraseAll()) {
    ESP_LOGE(TAG, "self-test cleanup: eraseAll failed");
    return false;
  }
  app_ = std::make_unique<LearningApp>(storage_, clock_);
  app_->setIdentity(claw4::domain::DeviceId{kDeviceId},
                    claw4::domain::ChildId{kChildId});
  app_->setEventIdFactory([this] {
    return claw4::domain::EventId{NextEventId()};
  });
  app_->setSessionIdFactory([this] {
    return claw4::domain::SessionId{NextSessionId()};
  });
  const bool ok = app_->applyTodaySnapshot(DemoTodaySnapshot());
  app_->start();
  ESP_LOGI(TAG, "self-test cleanup: re-seeded=%d", ok ? 1 : 0);
  return ok;
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
