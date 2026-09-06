// CODEX-APP-FIRST-001 AF3 — reusable device backend session.
//
// This class is the narrow bridge between BackendClient and AppCoordinator:
// authenticate once, apply the authoritative today snapshot, then drain the
// transactional outbox. It owns no thread and never sleeps; a device shell
// calls runOnlineCycle() from a worker/task context, not from LVGL callbacks.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "application/coordinator.h"
#include "learning_domain/ids.h"
#include "sync/backend_client.h"
#include "sync/backend_sync_transport.h"

namespace claw4::sync {

struct BackendSessionDiagnostics {
  bool configured = false;
  bool authenticated = false;
  bool network_online = false;
  bool auth_paused = false;
  int pending_count = 0;
  int64_t last_acked_sequence = 0;
  int64_t last_sync_epoch = 0;
  int http_status = 0;
  SyncErrorClass last_error = SyncErrorClass::None;
  std::string last_operation;
};

// BackendClient remains the only owner of endpoint JSON and HTTP semantics.
// This session only sequences the already-tested operations and projects a
// redacted diagnostic snapshot for the device UI.
class LearningBackendSession final {
 public:
  using Signer = BackendClient::ChallengeSigner;
  using EpochNow = std::function<int64_t()>;

  LearningBackendSession(application::AppCoordinator& app,
                         std::unique_ptr<HttpTransport> transport,
                         std::string base_url,
                         domain::DeviceId device_id,
                         domain::ChildId child_id,
                         Signer signer,
                         EpochNow epoch_now = {});

  // Returns false without touching the network when endpoint or signer is
  // absent. Credentials are supplied by the caller; this class never embeds
  // or logs a device secret.
  bool authenticate();
  bool pullToday();
  application::SyncOutcome syncOnce();
  bool runOnlineCycle();

  const BackendSessionDiagnostics& diagnostics() const { return diagnostics_; }
  bool authenticated() const { return authenticated_; }

 private:
  void RefreshCounters();
  void RecordError(SyncErrorClass error, int http_status,
                   const char* operation);
  bool EnsureAuthenticated();

  application::AppCoordinator& app_;
  std::unique_ptr<HttpTransport> transport_;
  BackendClient client_;
  BackendSyncTransport sync_transport_;
  std::string base_url_;
  domain::DeviceId device_id_;
  domain::ChildId child_id_;
  Signer signer_;
  EpochNow epoch_now_;
  bool authenticated_ = false;
  BackendSessionDiagnostics diagnostics_;
};

}  // namespace claw4::sync
