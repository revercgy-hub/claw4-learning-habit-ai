// claw4/firmware/main/sync/outbox_core.h
// Platform-independent transactional outbox core (WB-STREAM-002 CP2).
// Portable C++17. No hardware dependencies. Single-threaded by design; real
// device concurrency is out of scope.
//
// Responsibilities:
//   * Atomic transition persistence (domain snapshot + events + counters)
//   * Sole per-device sequence allocator (reducers/UI never own sequences)
//   * Pending capacity budget (<= 200 rows), critical events never dropped
//   * Mergeable local diagnostic slot (sync.failed / sync.recovered)
//   * ACK cleanup (Accepted/Duplicate inside the consecutive prefix only)
//   * Dead-letter markers for business 4xx (original rows kept for replay)
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"
#include "learning_domain/ids.h"
#include "sync/batch_result.h"
#include "sync/event_sink.h"
#include "sync/outbox_storage.h"

namespace claw4 {
namespace sync {

// Event types that must never be silently dropped by a capacity policy.
bool isCriticalEvent(claw4::domain::EventType type);

class OutboxCore final : public EventSink {
 public:
  static constexpr int kMaxPending = 200;

  // The storage is owned by the caller and must outlive this core. Core does
  // not cache mutable copies: every operation reloads from storage to stay
  // consistent with the storage's committed state.
  explicit OutboxCore(OutboxStorage* storage);

  // EventSink: atomically allocates consecutive sequences, materializes the
  // drafts into PendingEvent rows and persists snapshot + rows + counter.
  // Failure commits nothing and consumes no sequence.
  PersistResult persistTransition(const PendingTransition& transition) override;

  // Marks the mergeable diagnostic slot. Never touches the business queue.
  PersistResult setDiagnostic(bool sync_failed, bool sync_recovered);

  // Applies a batch sync response: only rows that are Accepted/Duplicate AND
  // inside the consecutive ACK prefix (sequence <= last_acked_sequence) are
  // removed. Conflict/Rejected/Gap rows are kept. Business-4xx rejected rows
  // additionally get a dead-letter marker (original row retained). Auth,
  // network and 5xx outcomes never remove anything.
  PersistResult applyBatchResult(const BatchSyncResult& result);

  // A05-DEVICE-T1: re-base every still-pending row above `new_base` (the
  // baseline the server announced) so a queue blocked by permanently
  // un-ackable rows can be delivered instead of being re-sent forever.
  // Lossless: no event_id or payload is dropped. Atomic: delegated to
  // OutboxStorage::rebaseSequences(). Returns StorageError when the storage
  // cannot do it (fail-closed), in which case the caller must report a blocked
  // queue rather than a successful sync.
  PersistResult rebaseToServerBaseline(int64_t new_base);

  // Convenience: number of pending rows + whether the diagnostic slot is set.
  int pendingCount() const;
  int64_t nextSequence() const;
  int64_t lastAcked() const;

 private:
  OutboxStorage* storage_;
  bool loadState(OutboxState& out) const;
  PersistResult makeFailure(PersistStatus status) const;
};

}  // namespace sync
}  // namespace claw4
