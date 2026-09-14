// claw4/firmware/main/sync/sync_executor.h
// WB-V53-NEXT-001 CP1 (A03): the single production sync cycle that keeps the
// network wait OUT of the state-owner critical section.
//
// The cycle is three explicit phases (ADR V53_A03_RUNTIME_CONCURRENCY_ADR §2):
//
//   phase 1  LOCKED    AppCoordinator::prepareSync()  -> immutable envelope
//   phase 2  UNLOCKED  SyncTransport::send()          -> DNS/connect/read/close
//   phase 3  LOCKED    AppCoordinator::applySyncResult() -> generation/scope
//                                                        checked ACK + publish
//
// Local commands and UI snapshots therefore keep making progress while a
// request is suspended on a dead network. The credential refresh (re-auth) is
// also invoked with NO lock held; `applySyncResult()` itself never does I/O.
//
// The executor is platform-neutral (the lock is injected as two callables) so
// the device shell and the host gate exercise the SAME production code path:
// the device passes the same recursive mutex its UI reads use, the host test
// passes a std::mutex plus a latch-blocked transport.
//
// REVIEW-FIX-002 R7.2/R7.3: the executor is BOUND to one session generation at
// construction. Every attempt re-checks it, so a credential retry can never
// "upgrade" a dying session to the generation of a session that replaced it.
#pragma once

#include <cstdint>
#include <functional>

#include "application/coordinator.h"

namespace claw4 {
namespace sync {

// Acquire / release the state-owner lock. The device injects
// `[this]{ state_mutex_.lock(); }` / `[this]{ state_mutex_.unlock(); }`.
using StateLockFn = std::function<void()>;
using StateUnlockFn = std::function<void()>;

struct SyncCycleResult {
  application::SyncOutcome outcome = application::SyncOutcome::NoPending;
  bool transport_called = false;  // true when the network phase actually ran
  bool stale = false;             // a late result was rejected by generation
  int64_t generation = 0;         // generation captured by the last prepare
  int transport_calls = 0;
};

class SyncExecutor {
 public:
  // `expected_generation` is the session generation this executor was created
  // for (frozen by the owning LearningBackendSession at construction). It is
  // re-checked on EVERY attempt, including the one after a credential refresh.
  SyncExecutor(application::AppCoordinator& coord,
               application::SyncTransport& transport, StateLockFn lock,
               StateUnlockFn unlock, int64_t expected_generation);

  // Runs one full cycle. `max_attempts` bounds the number of network phases:
  // the extra attempt exists only for the one permitted credential retry.
  SyncCycleResult runCycle(const application::ReauthFn& reauth,
                           int max_attempts = 2);

 private:
  // True while this session still owns the coordinator state. Only callable
  // inside the state critical section.
  bool generationCurrentLocked() const;

  application::AppCoordinator& coord_;
  application::SyncTransport& transport_;
  StateLockFn lock_;
  StateUnlockFn unlock_;
  int64_t expected_generation_ = 0;
};

}  // namespace sync
}  // namespace claw4
