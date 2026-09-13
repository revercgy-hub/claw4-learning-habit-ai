// claw4/integration/metalio_claw4/device/app/learning_runtime.cpp
// WB-LEARNING-V4-L1 — device LearningApp runtime (see header).
#include "metalio_claw4/device/app/learning_runtime.h"

#include <memory>
#include <mutex>
#include <utility>

#include "esp_log.h"
#include "esp_random.h"
#include "metalio_claw4/device/core/demo_seed.h"
#include "metalio_claw4/device/core/learning_boot_policy.h"
#include "metalio_claw4/device/core/outbox_codec.h"
#include "metalio_claw4/device/core/random_id.h"
#include "metalio_claw4/device/ports/metalio_http_transport.h"
#include "metalio_claw4/device/ports/metalio_hmac_signer.h"
#include "metalio_claw4/device/ports/nvs_backend_provisioning.h"

namespace claw4 {
namespace metalio {
namespace {

constexpr const char* TAG = "LearningRt";

}  // namespace

LearningRuntime& LearningRuntime::Instance() {
  static LearningRuntime rt;
  return rt;
}

std::string LearningRuntime::NextEventId() {
  // Two independent 32-bit draws, formatted without printf-family 64-bit
  // specifiers (unsupported by the firmware's nano-newlib configuration).
  return formatEntropyId("ev-", esp_random(), esp_random());
}

std::string LearningRuntime::NextSessionId() {
  return formatEntropyId("sess-", esp_random(), esp_random());
}

bool LearningRuntime::ConfigureBackend(
    std::string base_url, std::string device_id, std::string child_id,
    claw4::sync::LearningBackendSession::Signer signer) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  if (!bootReady()) return false;
  device_id_ = device_id;
  child_id_ = child_id;
  app_->setIdentity(claw4::domain::DeviceId{device_id_},
                    claw4::domain::ChildId{child_id_});
  backend_ = std::make_unique<claw4::sync::LearningBackendSession>(
      app_->coordinator(), CreateMetalioHttpTransport(), std::move(base_url),
      claw4::domain::DeviceId{std::move(device_id)},
      claw4::domain::ChildId{std::move(child_id)},
      std::move(signer), [this] { return clock_.epochSeconds(); });
  return backend_->diagnostics().configured;
}

bool LearningRuntime::ConfigureProvisionedBackend() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  NvsBackendProvisioning provisioning;
  claw4::sync::ProvisionedBackendConfig config;
  if (provisioning.load(config) != claw4::sync::ProvisioningStatus::Ready) {
    return false;
  }
  device_id_ = config.device_id;
  child_id_ = config.child_id;
  if (app_ != nullptr) {
    app_->setIdentity(claw4::domain::DeviceId{device_id_},
                      claw4::domain::ChildId{child_id_});
  }
  return ConfigureBackend(
      std::move(config.base_url), std::move(config.device_id),
      std::move(config.child_id),
      CreateMetalioHmacSigner(std::move(config.device_secret)));
}

bool LearningRuntime::RunOnlineCycle() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  if (backend_ == nullptr) return false;
  // The caller owns scheduling: this method must not be invoked from the
  // LVGL event callback because the scheduled HTTP boundary waits for its
  // main-loop callback to complete.
  return backend_->runOnlineCycle();
}

claw4::sync::BackendSessionDiagnostics LearningRuntime::BackendDiagnostics() const {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  return backend_ ? backend_->diagnostics()
                  : claw4::sync::BackendSessionDiagnostics{};
}

void LearningRuntime::StartBackendWorker() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  if (backend_task_ != nullptr) return;
  const BaseType_t rc = xTaskCreate(
      [](void* arg) {
        auto* runtime = static_cast<LearningRuntime*>(arg);
        for (;;) {
          bool has_backend = false;
          {
            std::lock_guard<std::recursive_mutex> lock(runtime->state_mutex_);
            has_backend = runtime->backend_ != nullptr;
          }
          if (!has_backend) {
            runtime->ConfigureProvisionedBackend();
          }
          runtime->RunOnlineCycle();
          vTaskDelay(pdMS_TO_TICKS(15000));
        }
      },
      "learning_backend", 8192, this, 4, &backend_task_);
  if (rc != pdPASS) {
    backend_task_ = nullptr;
    ESP_LOGE(TAG, "backend worker start failed");
  }
}

void LearningRuntime::Init() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  if (inited_) return;
  NvsBackendProvisioning provisioning;
  claw4::sync::ProvisionedBackendConfig provisioned;
  const auto provisioning_status = provisioning.load(provisioned);
  ProvisioningDisposition disposition =
      ProvisioningDisposition::InvalidOrUnavailable;
  if (provisioning_status == claw4::sync::ProvisioningStatus::Ready) {
    disposition = ProvisioningDisposition::Provisioned;
    device_id_ = provisioned.device_id;
    child_id_ = provisioned.child_id;
  } else if (provisioning_status ==
             claw4::sync::ProvisioningStatus::NotConfigured) {
    disposition = ProvisioningDisposition::Unprovisioned;
  }
  app_ = std::make_unique<LearningApp>(storage_, clock_);
  app_->setIdentity(claw4::domain::DeviceId{device_id_},
                    claw4::domain::ChildId{child_id_});
  app_->setEventIdFactory([this] {
    return claw4::domain::EventId{NextEventId()};
  });
  app_->setSessionIdFactory([this] {
    return claw4::domain::SessionId{NextSessionId()};
  });
  // A reboot resets esp_timer's monotonic epoch. Rebase any persisted live
  // session before exposing the app to the screen; on failure keep the app
  // stopped so an uncommitted recovery can never be presented as success.
  if (!app_->coordinator().prepareAfterBoot(clock_.monotonicMs())) {
    ESP_LOGE(TAG, "boot recovery failed: keeping LearningApp stopped");
    app_.reset();
    return;
  }
  claw4::sync::OutboxState persisted;
  if (!storage_.load(persisted)) {
    ESP_LOGE(TAG, "learning state read failed: keeping LearningApp stopped");
    app_.reset();
    return;
  }
  const LearningBootAction boot_action = DecideLearningBootAction(
      disposition, storage_.hasState());
  if (boot_action == LearningBootAction::ClosedBootGate) {
    ESP_LOGE(TAG,
             "provisioning unavailable/invalid: keeping LearningApp stopped");
    app_.reset();
    return;
  }
  if (boot_action == LearningBootAction::SeedDemo) {
    const bool ok = app_->applyTodaySnapshot(DemoTodaySnapshot());
    ESP_LOGI(TAG, "first boot: demo today snapshot seeded=%d", ok ? 1 : 0);
  } else {
    ESP_LOGI(TAG,
             "boot with committed state: tasks=%u pending=%d active=%d",
             static_cast<unsigned>(app_->state().tasks.size()),
             app_->pendingCount(),
             app_->state().active_session.has_value() ? 1 : 0);
  }
  app_->start();
  inited_ = true;
  // Configuration is local-only and does not contact the network. The worker
  // performs challenge/auth, today pull, and outbox event sync off the LVGL
  // callback thread once the page is created.
  ConfigureProvisionedBackend();
  StartBackendWorker();
}

}  // namespace metalio
}  // namespace claw4
