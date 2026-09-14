// claw4/firmware/main/sync/sync_executor.cpp
#include "sync/sync_executor.h"

#include <utility>

namespace claw4 {
namespace sync {
namespace {

// RAII wrapper over the injected lock callables. Every critical section in this
// file is a short, I/O-free transaction.
class ScopedStateLock {
 public:
  ScopedStateLock(const StateLockFn& lock, const StateUnlockFn& unlock)
      : unlock_(unlock) {
    lock();
  }
  ~ScopedStateLock() { unlock_(); }
  ScopedStateLock(const ScopedStateLock&) = delete;
  ScopedStateLock& operator=(const ScopedStateLock&) = delete;

 private:
  const StateUnlockFn& unlock_;
};

}  // namespace

SyncExecutor::SyncExecutor(application::AppCoordinator& coord,
                           application::SyncTransport& transport,
                           StateLockFn lock, StateUnlockFn unlock)
    : coord_(coord),
      transport_(transport),
      lock_(std::move(lock)),
      unlock_(std::move(unlock)) {}

SyncCycleResult SyncExecutor::runCycle(const application::ReauthFn& reauth,
                                      int max_attempts) {
  SyncCycleResult result;
  if (max_attempts < 1) max_attempts = 1;

  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    // ---- phase 1: short, I/O-free prepare under the lock ----------------
    application::SyncRequestEnvelope envelope;
    {
      ScopedStateLock guard(lock_, unlock_);
      envelope = coord_.prepareSync();
    }
    result.generation = envelope.generation;

    if (envelope.auth_paused) {
      // Transport short-circuit (FIX-V4-01): no send, no re-auth.
      result.outcome = application::SyncOutcome::PausedAuth;
      return result;
    }
    if (!envelope.has_work) {
      result.outcome = application::SyncOutcome::NoPending;
      return result;
    }

    // ---- phase 2: network with NO state lock held -----------------------
    result.transport_called = true;
    ++result.transport_calls;
    const SyncClient::Response response = transport_.send(envelope.request);

    // ---- phase 3: short, I/O-free apply under the lock ------------------
    application::SyncApplyOutcome applied;
    {
      ScopedStateLock guard(lock_, unlock_);
      applied = coord_.applySyncResult(envelope, response);
    }
    result.outcome = applied.outcome;
    result.stale = applied.stale;

    if (applied.outcome != application::SyncOutcome::ReauthOk) return result;

    // Credential refresh runs with NO state lock held; only this one extra
    // attempt is allowed, matching the legacy runSyncOnce budget.
    if (reauth && reauth()) continue;

    // Refresh failed: re-apply the same auth-failing response so the
    // coordinator reaches its pause state (no network, no re-auth call).
    application::SyncApplyOutcome paused;
    {
      ScopedStateLock guard(lock_, unlock_);
      paused = coord_.applySyncResult(envelope, response);
    }
    result.outcome = paused.outcome;
    result.stale = paused.stale;
    return result;
  }
  return result;
}

}  // namespace sync
}  // namespace claw4
