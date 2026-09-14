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
                           StateLockFn lock, StateUnlockFn unlock,
                           const int64_t expected_generation)
    : coord_(coord),
      transport_(transport),
      lock_(std::move(lock)),
      unlock_(std::move(unlock)),
      expected_generation_(expected_generation) {}

bool SyncExecutor::generationCurrentLocked() const {
  return coord_.generation() == expected_generation_;
}

SyncCycleResult SyncExecutor::runCycle(const application::ReauthFn& reauth,
                                      int max_attempts) {
  SyncCycleResult result;
  if (max_attempts < 1) max_attempts = 1;
  result.generation = expected_generation_;

  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    // ---- phase 1: short, I/O-free prepare under the lock ----------------
    application::SyncRequestEnvelope envelope;
    {
      ScopedStateLock guard(lock_, unlock_);
      // R7.2: ownership gate BEFORE preparing. A session that was superseded
      // (reconfigure / teardown) must not even build a request, and the retry
      // after a credential refresh must be re-gated exactly the same way so it
      // can never pick up the NEW generation.
      if (!generationCurrentLocked()) {
        result.outcome = application::SyncOutcome::StaleResult;
        result.stale = true;
        return result;
      }
      envelope = coord_.prepareSync();
    }

    if (envelope.auth_paused) {
      // Transport short-circuit (FIX-V4-01): no send, no re-auth.
      result.outcome = application::SyncOutcome::PausedAuth;
      return result;
    }
    if (!envelope.has_work) {
      result.outcome = application::SyncOutcome::NoPending;
      return result;
    }

    // The envelope must carry the frozen generation. If the coordinator stamped
    // anything else, this executor no longer owns the state: refuse before the
    // network phase, so a stale session never issues a new request.
    if (envelope.generation != expected_generation_) {
      result.outcome = application::SyncOutcome::StaleResult;
      result.stale = true;
      return result;
    }
    result.generation = envelope.generation;

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
    // attempt is allowed, matching the legacy runSyncOnce budget. The retry
    // itself is gated again at the top of the loop (R7.3), and the session's
    // own callback refuses to refresh for a superseded session.
    if (reauth && reauth()) continue;

    // Refresh failed OR was refused for a stale session. Re-apply the
    // auth-failing response so the coordinator reaches its pause state — but
    // only while this session still owns the state, otherwise a dying session
    // would pause the session that replaced it.
    application::SyncApplyOutcome paused;
    {
      ScopedStateLock guard(lock_, unlock_);
      if (!generationCurrentLocked()) {
        result.outcome = application::SyncOutcome::StaleResult;
        result.stale = true;
        return result;
      }
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
