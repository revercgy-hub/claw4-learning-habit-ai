// claw4/firmware/main/sync/event_sink.h
// EventSink: the boundary through which domain events enter the sync layer.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §3.4 / §7.3.
#pragma once

#include <cstdint>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"

namespace claw4 {
namespace sync {

// Persists a domain event into the local event queue together with the domain
// snapshot in one atomic sequence (outbox, ARCHITECTURE.md §7.3.1). The sync
// layer MUST NOT implement network or persistence here — this checkpoint only
// fixes the interface.
struct PendingTransition {
  claw4::domain::DomainState next_state;
  std::vector<claw4::domain::EventDraft> event_drafts;
};

enum class PersistStatus : uint8_t {
  Committed = 0,
  InvalidTransition,
  CapacityExceeded,
  StorageError,
};

struct PersistResult {
  PersistStatus status = PersistStatus::StorageError;
  // Fully materialized envelopes, including the sequences allocated by the
  // outbox transaction. Empty for every non-committed result.
  std::vector<claw4::domain::DeviceEvent> committed_events;

  // ACK-cleanup reporting (A05-DEVICE-T1 fix). Only meaningful for
  // OutboxCore::applyBatchResult(); zero/false everywhere else.
  //   acked_removed : how many pending rows this apply actually deleted
  //   ack_advanced  : whether the local last_acked_sequence moved forward
  // These exist so the sync layer can tell "committed, nothing to do" apart
  // from "committed, but the queue did not move" (a blocked queue must never be
  // reported as a successful sync).
  int acked_removed = 0;
  bool ack_advanced = false;

  bool committed() const noexcept { return status == PersistStatus::Committed; }
};

class EventSink {
 public:
  virtual ~EventSink() = default;

  // Atomically allocates consecutive sequences, materializes the drafts, and
  // persists the complete next domain snapshot + events + next-sequence
  // counter. Failure commits nothing and consumes no sequence. The caller
  // retries with the same event_id values; UI publication happens only after
  // PersistResult::committed() (I7/I8).
  virtual PersistResult persistTransition(const PendingTransition& transition) = 0;
};

}  // namespace sync
}  // namespace claw4
