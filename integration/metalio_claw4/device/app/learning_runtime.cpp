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
// FINAL-CONCURRENCY-CLEANUP FIX-1: shared atomic ownership-check + publish.
#include "sync/session_snapshot_publisher.h"

namespace claw4 {
namespace metalio {
namespace {

constexpr const char* TAG = "LearningRt";

}  // namespace

LearningRuntime& LearningRuntime::Instance() {
  static LearningRuntime rt;
  return rt;
}

LearningRuntime::LearningRuntime()
    : state_lock_([this] { state_mutex_.lock(); }),
      state_unlock_([this] { state_mutex_.unlock(); }) {}

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
  // WB-V53-NEXT-001 CP1 (A03) / RF1: a reconfigured backend is a NEW session.
  // Bump the coordinator generation AND install a fresh lease, so any worker
  // result prepared against the old session is rejected by applySyncResult()
  // and any in-flight old session is refused at its next gate.
  const int64_t generation = app_->coordinator().beginNewSession();
  backend_holder_.installWith(
      generation, [&](const claw4::sync::SessionLease& lease) {
        return std::make_shared<claw4::sync::LearningBackendSession>(
            app_->coordinator(), CreateMetalioHttpTransport(),
            std::move(base_url),
            claw4::domain::DeviceId{std::move(device_id)},
            claw4::domain::ChildId{std::move(child_id)},
            std::move(signer), [this] { return clock_.epochSeconds(); }, lease,
            this);
      });
  const auto backend = backend_holder_.borrow();
  return backend != nullptr && backend->diagnostics().configured;
}

claw4::sync::SessionLease LearningRuntime::currentLease() const {
  return backend_holder_.currentLease();
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
  // WB-V53-NEXT-001 CP1 (A03) + REVIEW-FIX-001 RF1: the state lock is held ONLY
  // for short, I/O-free transactions, and the session object is kept alive for
  // the whole network phase by the lease holder's shared_ptr borrow (a bare
  // pointer taken inside the lock would not survive ConfigureBackend()
  // replacing the session).
  const std::shared_ptr<claw4::sync::LearningBackendSession> backend =
      backend_holder_.borrow();
  if (backend == nullptr) return false;

  // Phase B: perform the cycle with NO state lock held. The session takes the
  // injected lock only for its own short prepare/apply writes, and refuses to
  // send or apply anything once it has been superseded.
  const bool ok = backend->runOnlineCycle(state_lock_, state_unlock_);

  // Phase C: publish the immutable diagnostics snapshot the UI reads. The
  // ownership check and the write are ONE state critical section (FIX-1): the
  // same pure-C++ helper the Host gate exercises holds the injected state lock
  // across `accepts()` + publish, so a reconfigure can no longer land between
  // them and let this dead session overwrite the new session's diagnostics.
  const claw4::sync::BackendSessionDiagnostics snapshot = backend->diagnostics();
  claw4::sync::publishSessionSnapshot(backend_holder_, *backend, state_lock_,
                                      state_unlock_, diagnostics_snapshot_,
                                      snapshot);
  return ok;
}

claw4::sync::BackendSessionDiagnostics LearningRuntime::BackendDiagnostics() const {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  return diagnostics_snapshot_;
}

void LearningRuntime::StartBackendWorker() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  if (backend_task_ != nullptr) return;
  const BaseType_t rc = xTaskCreate(
      [](void* arg) {
        auto* runtime = static_cast<LearningRuntime*>(arg);
        for (;;) {
          // The lease holder is internally synchronized; no state lock needed.
          const bool has_backend = runtime->backend_holder_.borrow() != nullptr;
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
  if (disposition == ProvisioningDisposition::InvalidOrUnavailable) {
    ESP_LOGE(TAG, "provisioning unavailable: learning boot gate closed");
    return;
  }
  claw4::sync::OutboxState persisted;
  NvsOutboxStorage::StateReadStatus storage_status =
      NvsOutboxStorage::StateReadStatus::Error;
  app_ = std::make_unique<LearningApp>(storage_, clock_);
  app_->setIdentity(claw4::domain::DeviceId{device_id_},
                    claw4::domain::ChildId{child_id_});
  app_->setEventIdFactory([this] {
    return claw4::domain::EventId{NextEventId()};
  });
  app_->setSessionIdFactory([this] {
    return claw4::domain::SessionId{NextSessionId()};
  });
  const bool boot_ok = RunLearningBoot(
      disposition,
      [&] {
        storage_status = storage_.loadWithPresence(persisted);
        if (storage_status == NvsOutboxStorage::StateReadStatus::Error)
          return LearningStorageStatus::Error;
        return storage_status == NvsOutboxStorage::StateReadStatus::Present
                   ? LearningStorageStatus::Present
                   : LearningStorageStatus::Missing;
      },
      [&] {
        return app_->coordinator().prepareAfterBoot(clock_.monotonicMs());
      },
      [&] {
        const bool ok = app_->applyTodaySnapshot(DemoTodaySnapshot());
        ESP_LOGI(TAG, "first boot: demo today snapshot seeded=%d", ok ? 1 : 0);
        return ok;
      },
      [&] {
        if (storage_status == NvsOutboxStorage::StateReadStatus::Present) {
          ESP_LOGI(TAG,
                   "boot with committed state: tasks=%u pending=%d active=%d",
                   static_cast<unsigned>(app_->state().tasks.size()),
                   app_->pendingCount(),
                   app_->state().active_session.has_value() ? 1 : 0);
        }
        app_->start();
      });
  if (!boot_ok) {
    ESP_LOGE(TAG, "learning boot gate closed");
    app_.reset();
    return;
  }
  inited_ = true;
  // Configuration is local-only and does not contact the network. The worker
  // performs challenge/auth, today pull, and outbox event sync off the LVGL
  // callback thread once the page is created.
  ConfigureProvisionedBackend();
  StartBackendWorker();
}

}  // namespace metalio
}  // namespace claw4
