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
    domain::ChildId child_id, Signer signer, EpochNow epoch_now)
    : app_(app),
      transport_(transport ? std::move(transport)
                           : std::make_unique<NullHttpTransport>()),
      client_(*transport_, base_url),
      sync_transport_(client_),
      base_url_(std::move(base_url)),
      device_id_(std::move(device_id)),
      child_id_(std::move(child_id)),
      signer_(std::move(signer)),
      epoch_now_(std::move(epoch_now)) {
  diagnostics_.configured = !base_url_.empty() && static_cast<bool>(signer_);
  RefreshCounters();
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
  if (!EnsureAuthenticated()) return false;
  wire::TodayResponse today;
  SyncErrorClass error = SyncErrorClass::None;
  if (!client_.fetchToday(child_id_.value, today, error)) {
    if (error == SyncErrorClass::Auth) authenticated_ = false;
    RecordError(error, 0, "today");
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
  if (!EnsureAuthenticated()) return application::SyncOutcome::Backoff;
  const auto outcome = app_.runSyncOnce(
      sync_transport_, [this]() { return authenticate(); });
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
  // Short critical section: the ONLY state write of the pull.
  bool applied = false;
  lock();
  applied = app_.applyTodaySnapshot(today.tasks);
  unlock();
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
  if (!EnsureAuthenticated()) {
    RefreshCountersLocked(lock, unlock);
    return application::SyncOutcome::Backoff;
  }
  // SyncExecutor owns the three-phase discipline: prepare (locked), send (NO
  // lock), apply (locked). The credential retry is also invoked unlocked.
  SyncExecutor executor(app_, sync_transport_, lock, unlock);
  const SyncCycleResult cycle =
      executor.runCycle([this]() { return authenticate(); });

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
  const auto outcome = syncOnceLocked(lock, unlock);
  return outcome == application::SyncOutcome::Synced ||
         outcome == application::SyncOutcome::NoPending;
}

}  // namespace claw4::sync
