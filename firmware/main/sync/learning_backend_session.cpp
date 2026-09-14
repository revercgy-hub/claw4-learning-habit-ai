#include "sync/learning_backend_session.h"

namespace claw4::sync {
namespace {

class NullHttpTransport final : public HttpTransport {
 public:
  HttpResponse request(const std::string&, const std::string&,
                       const std::vector<std::pair<std::string, std::string>>&,
                       const std::string&, int64_t) override {
    return {};
  }
};

}  // namespace

LearningBackendSession::LearningBackendSession(
    application::AppCoordinator& app, std::unique_ptr<HttpTransport> transport,
    std::string base_url, domain::DeviceId device_id,
    domain::ChildId child_id, Signer signer, EpochNow epoch_now,
    SessionLease lease, const SessionLeaseSource* lease_source)
    : app_(app),
      transport_(transport ? std::move(transport)
                           : std::make_unique<NullHttpTransport>()),
      client_(*transport_, base_url),
      sync_transport_(client_),
      base_url_(std::move(base_url)),
      device_id_(std::move(device_id)),
      child_id_(std::move(child_id)),
      signer_(std::move(signer)),
      epoch_now_(std::move(epoch_now)),
      lease_(lease),
      lease_source_(lease_source),
      session_generation_(lease.generation) {
  diagnostics_.configured = !base_url_.empty() && static_cast<bool>(signer_);
  RefreshCounters();
}

// RF1: only ONE session instance may mutate coordinator state. A session is
// superseded as soon as the runtime hands out a newer lease (reconfigure /
// session rebuild), or as soon as the coordinator generation moved on.
//
// REVIEW-FIX-002 R7.4: this predicate is called WITHOUT the state lock (the
// fast reject before a network phase), so it may only read thread-safe state:
//   * the lease source, which is internally synchronized, and
//   * the coordinator's ATOMIC generation.
// The plain-integer read that used to be here was an unsynchronized data race
// with beginNewSession(). The AUTHORITATIVE ownership decision for a state
// mutation is still taken inside the state critical section (see
// pullTodayLocked / SyncExecutor::generationCurrentLocked) using the frozen
// session_generation_, so the fast path is only an optimization.
bool LearningBackendSession::superseded() const {
  if (!leaseStillCurrent()) return true;
  return session_generation_ != app_.generation();
}

bool LearningBackendSession::leaseStillCurrent() const {
  // Lock order: when this runs under the state lock the holder's mutex is the
  // INNER lock (state -> lease), which is the same order
  // LearningRuntime::ConfigureBackend() uses when it takes state_mutex_ and
  // then installs the new lease. No reverse order exists anywhere.
  return lease_source_ == nullptr || lease_source_->isCurrent(lease_);
}

void LearningBackendSession::RefreshCounters() {
  diagnostics_.authenticated = authenticated_ && !client_.accessToken().empty();
  diagnostics_.auth_paused = app_.authPaused();
  diagnostics_.pending_count = app_.pendingCount();
  diagnostics_.last_acked_sequence = app_.lastAcked();
}

void LearningBackendSession::RefreshCountersLocked(
    const StateLockFn& lock, const StateUnlockFn& unlock) {
  // Counter reads go through the storage adapter, so they belong to the same
  // short critical section as the state writes they describe.
  lock();
  RefreshCounters();
  unlock();
}

void LearningBackendSession::RecordError(const SyncErrorClass error,
                                         const int http_status,
                                         const char* operation) {
  diagnostics_.network_online = false;
  diagnostics_.last_error = error;
  diagnostics_.http_status = http_status;
  diagnostics_.last_operation = operation;
  RefreshCounters();
}

void LearningBackendSession::RecordErrorLocked(
    const SyncErrorClass error, const int http_status, const char* operation,
    const StateLockFn& lock, const StateUnlockFn& unlock) {
  diagnostics_.network_online = false;
  diagnostics_.last_error = error;
  diagnostics_.http_status = http_status;
  diagnostics_.last_operation = operation;
  RefreshCountersLocked(lock, unlock);
}

bool LearningBackendSession::authenticate() {
  if (!diagnostics_.configured) {
    RecordError(SyncErrorClass::Unknown, 0, "auth_not_configured");
    return false;
  }
  wire::AuthResponse auth;
  SyncErrorClass error = SyncErrorClass::None;
  if (!client_.authenticate(device_id_.value, signer_, auth, error)) {
    authenticated_ = false;
    RecordError(error, 0, "authenticate");
    return false;
  }
  authenticated_ = true;
  diagnostics_.network_online = true;
  diagnostics_.last_error = SyncErrorClass::None;
  diagnostics_.http_status = 200;
  diagnostics_.last_operation = "authenticate";
  RefreshCounters();
  return true;
}

bool LearningBackendSession::EnsureAuthenticated() {
  return authenticated_ || authenticate();
}

bool LearningBackendSession::pullToday() {
  if (superseded()) return false;  // RF1: no new request from a dead session
  if (!EnsureAuthenticated()) return false;
  wire::TodayResponse today;
  SyncErrorClass error = SyncErrorClass::None;
  if (!client_.fetchToday(child_id_.value, today, error)) {
    if (error == SyncErrorClass::Auth) authenticated_ = false;
    RecordError(error, 0, "today");
    return false;
  }
  // RF1: the response may have been in flight while the runtime reconfigured.
  // A superseded session must NOT write the snapshot into the new session.
  if (superseded()) {
    diagnostics_.last_operation = "today_superseded";
    return false;
  }
  if (!app_.applyTodaySnapshot(today.tasks)) {
    RecordError(SyncErrorClass::Unknown, 0, "today_persist");
    return false;
  }
  diagnostics_.network_online = true;
  diagnostics_.last_error = SyncErrorClass::None;
  diagnostics_.http_status = 200;
  diagnostics_.last_operation = "today";
  RefreshCounters();
  return true;
}

application::SyncOutcome LearningBackendSession::syncOnce() {
  if (superseded()) return application::SyncOutcome::StaleResult;  // RF1
  if (!EnsureAuthenticated()) return application::SyncOutcome::Backoff;
  const auto outcome = app_.runSyncOnce(
      sync_transport_, [this]() {
        // RF1: never spend a credential refresh on a dead session.
        if (superseded()) return false;
        return authenticate();
      });
  diagnostics_.network_online = outcome != application::SyncOutcome::Backoff;
  diagnostics_.auth_paused = app_.authPaused();
  diagnostics_.last_operation = "events";
  if (outcome == application::SyncOutcome::Backoff) {
    diagnostics_.last_error = SyncErrorClass::Network;
  } else if (outcome == application::SyncOutcome::PausedAuth) {
    diagnostics_.last_error = SyncErrorClass::Auth;
  } else {
    diagnostics_.last_error = SyncErrorClass::None;
    if (epoch_now_) diagnostics_.last_sync_epoch = epoch_now_();
  }
  RefreshCounters();
  return outcome;
}

bool LearningBackendSession::runOnlineCycle() {
  if (!pullToday()) return false;
  const auto outcome = syncOnce();
  return outcome == application::SyncOutcome::Synced ||
         outcome == application::SyncOutcome::NoPending;
}

// ---------------------------------------------------------------------------
// WB-V53-NEXT-001 CP1 (A03): lock-injected cycle. Every network wait below runs
// with NO lock held; only the pure state writes and the counter reads are
// wrapped in the injected short critical section.
// ---------------------------------------------------------------------------

bool LearningBackendSession::pullTodayLocked(const StateLockFn& lock,
                                             const StateUnlockFn& unlock) {
  // RF1: a superseded session must not start a NEW /today request.
  if (superseded()) {
    diagnostics_.last_operation = "today_superseded";
    return false;
  }
  // authenticate() performs the challenge/auth HTTP exchange — no lock held.
  if (!EnsureAuthenticated()) {
    RefreshCountersLocked(lock, unlock);
    return false;
  }
  wire::TodayResponse today;
  SyncErrorClass error = SyncErrorClass::None;
  // GET /today: network wait with no lock held.
  if (!client_.fetchToday(child_id_.value, today, error)) {
    if (error == SyncErrorClass::Auth) authenticated_ = false;
    RecordErrorLocked(error, 0, "today", lock, unlock);
    return false;
  }
  // R7.1: the final ownership re-check and the state write are ONE critical
  // section. Previously the check ran BEFORE taking the state lock, which left
  // a legal interleaving:
  //     old: superseded() == false
  //                          new: ConfigureBackend() bumps the generation
  //     old: lock(); applyTodaySnapshot(stale_today)
  // The coordinator generation (the final truth for state mutation) and the
  // lease (object liveness) are both evaluated inside the transaction, and the
  // only writer of the generation — ConfigureBackend() — must hold this same
  // state lock before it can bump it, so it can no longer interleave.
  bool applied = false;
  bool rejected = false;
  lock();
  if (app_.generation() != session_generation_ || !leaseStillCurrent()) {
    rejected = true;
  } else {
    applied = app_.applyTodaySnapshot(today.tasks);
  }
  unlock();
  if (rejected) {
    // A late/foreign snapshot: nothing was written and this is not an error.
    diagnostics_.last_operation = "today_superseded";
    return false;
  }
  if (!applied) {
    RecordErrorLocked(SyncErrorClass::Unknown, 0, "today_persist", lock, unlock);
    return false;
  }
  diagnostics_.network_online = true;
  diagnostics_.last_error = SyncErrorClass::None;
  diagnostics_.http_status = 200;
  diagnostics_.last_operation = "today";
  RefreshCountersLocked(lock, unlock);
  return true;
}

application::SyncOutcome LearningBackendSession::syncOnceLocked(
    const StateLockFn& lock, const StateUnlockFn& unlock) {
  // RF1: gate the event sync on liveness BOTH before authenticating and again
  // before the exchange starts, so a session superseded between /today and
  // /events can never send with the new generation.
  if (superseded()) {
    RefreshCountersLocked(lock, unlock);
    return application::SyncOutcome::StaleResult;
  }
  if (!EnsureAuthenticated()) {
    RefreshCountersLocked(lock, unlock);
    return application::SyncOutcome::Backoff;
  }
  if (superseded()) {
    RefreshCountersLocked(lock, unlock);
    return application::SyncOutcome::StaleResult;
  }
  // SyncExecutor owns the three-phase discipline: prepare (locked), send (NO
  // lock), apply (locked). The credential retry is also invoked unlocked.
  // R7.2: the executor is BOUND to this session's frozen generation, so the
  // retry after a credential refresh is re-gated and can never adopt the
  // generation of a session that replaced this one.
  SyncExecutor executor(app_, sync_transport_, lock, unlock,
                        session_generation_);
  const SyncCycleResult cycle = executor.runCycle([this]() {
    // R7.3 credential-refresh lifecycle gate:
    //   (1) before authenticate: never spend a refresh on a dead session;
    //   (2) after the authenticate network wait: a reconfigure that landed
    //       while we were on the wire must stop the retry, so no second POST
    //       is ever issued and the new session's pause state is not touched.
    // (3) before the retry prepare: enforced by the executor's own generation
    //     gate at the top of the next attempt.
    if (superseded()) return false;
    if (!authenticate()) return false;
    return !superseded();
  });

  const auto outcome = cycle.outcome;
  diagnostics_.network_online = outcome != application::SyncOutcome::Backoff;
  RefreshCountersLocked(lock, unlock);
  diagnostics_.auth_paused = app_.authPaused();
  diagnostics_.last_operation = "events";
  if (outcome == application::SyncOutcome::Backoff) {
    diagnostics_.last_error = SyncErrorClass::Network;
  } else if (outcome == application::SyncOutcome::PausedAuth) {
    diagnostics_.last_error = SyncErrorClass::Auth;
  } else if (outcome == application::SyncOutcome::StaleResult) {
    // A late response from a superseded session: nothing was written and this
    // is not a network error.
    diagnostics_.last_error = SyncErrorClass::None;
  } else {
    diagnostics_.last_error = SyncErrorClass::None;
    if (epoch_now_) diagnostics_.last_sync_epoch = epoch_now_();
  }
  return outcome;
}

bool LearningBackendSession::runOnlineCycle(const StateLockFn& lock,
                                            const StateUnlockFn& unlock) {
  if (!pullTodayLocked(lock, unlock)) return false;
  // RF1: if the session was superseded during /today, do NOT continue into the
  // event sync.
  if (superseded()) return false;
  const auto outcome = syncOnceLocked(lock, unlock);
  return outcome == application::SyncOutcome::Synced ||
         outcome == application::SyncOutcome::NoPending;
}

}  // namespace claw4::sync
