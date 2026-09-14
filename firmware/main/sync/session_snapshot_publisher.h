// claw4/firmware/main/sync/session_snapshot_publisher.h
// WB-V53-NEXT-001 FINAL-CONCURRENCY-CLEANUP FIX-1:
// atomic "session ownership check + snapshot publish".
//
// The device runtime used to do:
//
//     const auto snapshot = backend->diagnostics();
//     if (backend_holder_.accepts(backend.get(), backend->lease())) {  // NO state lock
//         std::lock_guard<std::recursive_mutex> lock(state_mutex_);
//         diagnostics_snapshot_ = snapshot;                            // state lock
//     }
//
// The ownership check and the write were NOT in the same state transaction, so
// a reconfigure landing between them let a DEAD session publish its (stale)
// diagnostics over the new session's snapshot. Task state was never at risk —
// only the diagnostics the UI reads — but it is the same class of defect as the
// `/today` TOCTOU and is closed the same way: one critical section.
//
// WHY a shared helper: the device TU (LearningRuntime) is not part of the Host
// Gate, and a second copy of this rule inside it would be untested. The helper
// is a template over the lease source so that the DEVICE runtime and the HOST
// gate run the SAME code while each keeps its own mutex implementation.
//
// Lock order (unchanged, and the only order used anywhere):
//     state mutex   ->   SessionLeaseHolder mutex
// Never the reverse.
//
// Portable C++17. No LVGL / ESP-IDF / FreeRTOS / BSP dependencies.
#pragma once

#include "sync/session_lease.h"
#include "sync/sync_executor.h"  // StateLockFn / StateUnlockFn

namespace claw4 {
namespace sync {

// RAII over the injected lock callables. The `unlock` reference stays valid for
// the whole statement that owns the helper. Note this does NOT replace the
// production mutex: callers keep their own (recursive on the device).
class InjectedStateLock {
 public:
  InjectedStateLock(const StateLockFn& lock, const StateUnlockFn& unlock)
      : unlock_(unlock) {
    lock();
  }
  ~InjectedStateLock() { unlock_(); }
  InjectedStateLock(const InjectedStateLock&) = delete;
  InjectedStateLock& operator=(const InjectedStateLock&) = delete;

 private:
  const StateUnlockFn& unlock_;
};

// Publishes `snapshot` into `slot` IF AND ONLY IF `session` still owns the
// shared state, with both steps inside ONE state critical section. Returns true
// when the slot was written.
//
// `source` must expose
//     bool accepts(const SessionT*, const SessionLease&) const
// which both the production SessionLeaseHolder and the host-gate probe provide.
template <typename SourceT, typename SessionT, typename SnapshotT>
bool publishSessionSnapshot(const SourceT& source, const SessionT& session,
                            const StateLockFn& lock,
                            const StateUnlockFn& unlock, SnapshotT& slot,
                            const SnapshotT& snapshot) {
  InjectedStateLock guard(lock, unlock);
  if (!source.accepts(&session, session.lease())) return false;
  slot = snapshot;
  return true;
}

}  // namespace sync
}  // namespace claw4
