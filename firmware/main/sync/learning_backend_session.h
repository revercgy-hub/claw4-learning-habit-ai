// CODEX-APP-FIRST-001 AF3 — reusable device backend session.
//
// This class is the narrow bridge between BackendClient and AppCoordinator:
// authenticate once, apply the authoritative today snapshot, then drain the
// transactional outbox. It owns no thread and never sleeps; a device shell
// calls runOnlineCycle() from a worker/task context, not from LVGL callbacks.
//
// WB-V53-NEXT-001 CP1 (A03): the session now offers a lock-injected cycle so
// the device shell can keep the state-owner lock SHORT. In that mode every
// network call (challenge/auth, today pull, batch sync, the one credential
// retry) runs with NO lock held; only the two pure state writes
// (applyTodaySnapshot and the ACK/backoff application) take the injected lock.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "application/coordinator.h"
#include "learning_domain/ids.h"
#include "sync/backend_client.h"
#include "sync/backend_sync_transport.h"
#include "sync/session_lease.h"
#include "sync/sync_executor.h"

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
                         EpochNow epoch_now = {},
                         SessionLease lease = {},
                         const SessionLeaseSource* lease_source = nullptr);

  // Returns false without touching the network when endpoint or signer is
  // absent. Credentials are supplied by the caller; this class never embeds
  // or logs a device secret.
  bool authenticate();
  bool pullToday();
  application::SyncOutcome syncOnce();

  // Legacy single-threaded cycle (Host tests, wire fixtures). Performs no
  // locking; callers must guarantee exclusive access.
  bool runOnlineCycle();

  // A03 device cycle: all I/O happens with no lock held, every state write
  // happens inside a short injected critical section.
  bool runOnlineCycle(const StateLockFn& lock, const StateUnlockFn& unlock);

  const BackendSessionDiagnostics& diagnostics() const { return diagnostics_; }
  bool authenticated() const { return authenticated_; }

  // RF1: true when this session instance has been superseded by a newer one
  // (runtime reconfigured / session rebuilt). A superseded session must not
  // send NEW /today or events requests, must not apply a late response and must
  // not publish diagnostics over the new session's.
  bool superseded() const;
  SessionLease lease() const { return lease_; }

 private:
  void RefreshCounters();
  // Reads coordinator counters through the storage; must not run without the
  // state-owner lock while another task can commit.
  void RefreshCountersLocked(const StateLockFn& lock, const StateUnlockFn& unlock);
  void RecordError(SyncErrorClass error, int http_status, const char* operation);
  void RecordErrorLocked(SyncErrorClass error, int http_status,
                         const char* operation, const StateLockFn& lock,
                         const StateUnlockFn& unlock);
  bool EnsureAuthenticated();
  bool pullTodayLocked(const StateLockFn& lock, const StateUnlockFn& unlock);
  application::SyncOutcome syncOnceLocked(const StateLockFn& lock,
                                          const StateUnlockFn& unlock);

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
  // RF1: identity + liveness of this session instance.
  SessionLease lease_;
  const SessionLeaseSource* lease_source_ = nullptr;
};

}  // namespace claw4::sync
