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
//
// REVIEW-FIX-002 R7 (same package): the session generation is FROZEN at
// construction and every state mutation re-checks it INSIDE the state critical
// section, so
//   * a `/today` response can no longer be applied after a reconfigure
//     (the ownership check and applyTodaySnapshot are one transaction);
//   * a credential retry can never be performed with the NEW generation;
//   * a session superseded during its re-auth never issues a second POST.
//
// Lock order (both on device and in the host gate):
//     state mutex   ->   SessionLeaseHolder mutex
// `LearningRuntime::ConfigureBackend()` takes the state mutex before installing
// the new lease, and `leaseStillCurrent()` (the only place this class touches
// the holder) is called either with no lock or inside the state critical
// section. The reverse order never occurs, so there is no deadlock cycle.
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
  //
  // R7.4: safe to call WITHOUT the state lock — it only consults thread-safe
  // state (the lease source, and the coordinator's atomic generation).
  bool superseded() const;
  SessionLease lease() const { return lease_; }
  // R7.2: the generation this session froze at construction. It never changes,
  // so no retry can inherit the generation of a session that replaced this one.
  int64_t sessionGeneration() const { return session_generation_; }

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
  // R7.4: thread-safe object-liveness check. May be called with or without the
  // state lock; when called under it, the lock order is state -> lease holder.
  bool leaseStillCurrent() const;
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
  // R7.2: FROZEN at construction (== lease_.generation). Never re-read from the
  // coordinator, so a retry cannot adopt a newer session's generation.
  int64_t session_generation_ = 0;
};

}  // namespace claw4::sync
