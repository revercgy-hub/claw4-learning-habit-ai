// claw4/firmware/main/sync/outbox_core.cpp
// Transactional outbox core implementation (WB-STREAM-002 CP2).
#include "sync/outbox_core.h"

#include <algorithm>

namespace claw4 {
namespace sync {

namespace {
// MVP business events that must never be dropped by capacity policy.
constexpr bool kCritical[] = {
    true,   // DeviceBooted(0) - conservative
    false,  // DeviceOnline(1)
    false,  // DeviceOffline(2)
    true,   // TaskStarted(3)
    true,   // TaskPaused(4)
    true,   // TaskResumed(5)
    true,   // TaskCompleted(6)
    true,   // TaskSkipped(7)
    true,   // StudySessionStarted(8)
    true,   // StudySessionCompleted(9)
    false,  // SyncFailed(10) - diagnostic, separate slot
    false,  // SyncRecovered(11) - diagnostic, separate slot
};
static_assert(sizeof(kCritical) / sizeof(kCritical[0]) == 12,
              "kCritical table must cover every EventType value");
}  // namespace

bool isCriticalEvent(claw4::domain::EventType type) {
  const auto i = static_cast<unsigned>(type);
  return i < (sizeof(kCritical) / sizeof(kCritical[0])) && kCritical[i];
}

OutboxCore::OutboxCore(OutboxStorage* storage) : storage_(storage) {}

bool OutboxCore::loadState(OutboxState& out) const {
  return storage_ && storage_->load(out);
}

PersistResult OutboxCore::makeFailure(PersistStatus status) const {
  PersistResult r;
  r.status = status;
  return r;
}

PersistResult OutboxCore::persistTransition(const PendingTransition& transition) {
  if (!storage_) return makeFailure(PersistStatus::StorageError);

  OutboxState state;
  if (!loadState(state)) return makeFailure(PersistStatus::StorageError);

  // Invalid transitions (reducer never produces one, but guard anyway).
  if (transition.event_drafts.empty()) {
    // Pure state change (e.g. focus-segment timeout auto-pause): commit the
    // snapshot only, no sequence is consumed.
    const CommitStatus cs = storage_->commit(transition.next_state, {}, state.next_sequence);
    if (cs != CommitStatus::Committed) return makeFailure(PersistStatus::StorageError);
    PersistResult ok;
    ok.status = PersistStatus::Committed;
    return ok;
  }

  // Capacity policy: pending + drafts must fit the 200-row budget. Critical
  // events are never silently dropped — if the budget would be exceeded the
  // WHOLE transition fails and the old state stays committed.
  const int64_t required = static_cast<int64_t>(state.pending.size()) +
                           static_cast<int64_t>(transition.event_drafts.size());
  if (required > kMaxPending) {
    return makeFailure(PersistStatus::CapacityExceeded);
  }

  // Duplicate event_id protection: a draft whose id is already pending (e.g. a
  // wrongly repeated submit after a lost response) must not pollute the queue;
  // the caller re-syncs instead of re-persisting.
  for (const auto& existing : state.pending) {
    for (const auto& d : transition.event_drafts) {
      if (existing.event_id == d.event_id) {
        return makeFailure(PersistStatus::InvalidTransition);
      }
    }
  }

  // Sole sequence allocation: consecutive, starting from the persisted
  // counter. All materialized rows + the advanced counter are committed
  // atomically; on failure the counter is untouched (no holes).
  std::vector<PendingEvent> materialized;
  materialized.reserve(transition.event_drafts.size());
  int64_t seq = state.next_sequence;
  for (const auto& d : transition.event_drafts) {
    if (d.event_id.empty()) {
      return makeFailure(PersistStatus::InvalidTransition);
    }
    PendingEvent row;
    row.event_id = d.event_id;
    row.device_id = d.device_id;
    row.child_id = d.child_id;
    row.sequence = seq++;
    row.timestamp = d.timestamp;
    row.timestamp_source = d.timestamp_source;
    row.type = d.type;
    row.version = d.version;
    row.payload = d.payload;
    materialized.push_back(std::move(row));
  }

  const CommitStatus cs = storage_->commit(transition.next_state, materialized, seq);
  if (cs != CommitStatus::Committed) {
    return makeFailure(PersistStatus::StorageError);
  }

  PersistResult ok;
  ok.status = PersistStatus::Committed;
  for (const auto& row : materialized) {
    claw4::domain::DeviceEvent ev;
    ev.event_id = row.event_id;
    ev.device_id = row.device_id;
    ev.child_id = row.child_id;
    ev.sequence = row.sequence;
    ev.timestamp = row.timestamp;
    ev.timestamp_source = row.timestamp_source;
    ev.type = row.type;
    ev.version = row.version;
    ev.payload = row.payload;
    ok.committed_events.push_back(std::move(ev));
  }
  return ok;
}

PersistResult OutboxCore::setDiagnostic(bool sync_failed, bool sync_recovered) {
  if (!storage_) return makeFailure(PersistStatus::StorageError);
  const CommitStatus cs = storage_->commitDiagnostic(sync_failed, sync_recovered);
  if (cs != CommitStatus::Committed) return makeFailure(PersistStatus::StorageError);
  PersistResult ok;
  ok.status = PersistStatus::Committed;
  return ok;
}

PersistResult OutboxCore::applyBatchResult(const BatchSyncResult& result) {
  if (!storage_) return makeFailure(PersistStatus::StorageError);

  // (1) Dead-letter business 4xx rows first (keep original row + redacted
  //     reason; replayable with the same event_id). A persistence failure
  //     ABORTS the whole batch application (FIX-V4-03): no ACK cleanup, no
  //     pending deletion, no last_acked advance — the coordinator maps the
  //     StorageError to a Backoff so the events stay retryable after reboot.
  for (const auto& r : result.results) {
    if (r.outcome == EventOutcome::Rejected && r.http_status >= 400 &&
        r.http_status < 500 && r.http_status != 401 && r.http_status != 403) {
      const CommitStatus cs = storage_->markDeadLetter(
          r.event_id, "business_4xx_http=" + std::to_string(r.http_status));
      if (cs != CommitStatus::Committed) {
        return makeFailure(PersistStatus::StorageError);
      }
    }
  }

  // (2) ACK cleanup: only Accepted/Duplicate rows INSIDE the consecutive
  //     prefix (sequence <= last_acked_sequence) may be removed. Scan from the
  //     highest result downwards for the first eligible in-prefix row.
  //     Conflict/Rejected/Gap and anything past the prefix stays pending.
  auto rit = result.results.rbegin();
  for (; rit != result.results.rend(); ++rit) {
    const bool ok_outcome = rit->outcome == EventOutcome::Accepted ||
                            rit->outcome == EventOutcome::Duplicate;
    if (ok_outcome && rit->sequence <= result.last_acked_sequence) break;
  }
  if (rit == result.results.rend()) {
    // No eligible row inside the prefix; nothing to remove.
    PersistResult ok;
    ok.status = PersistStatus::Committed;
    return ok;
  }
  // Remove all pending with sequence <= the highest eligible in-prefix row.
  const int64_t up_to = rit->sequence;
  const CommitStatus cs = storage_->removeAcked(up_to);
  if (cs != CommitStatus::Committed) return makeFailure(PersistStatus::StorageError);

  PersistResult ok;
  ok.status = PersistStatus::Committed;
  return ok;
}

int OutboxCore::pendingCount() const {
  OutboxState s;
  return loadState(s) ? static_cast<int>(s.pending.size()) : 0;
}

int64_t OutboxCore::nextSequence() const {
  OutboxState s;
  return loadState(s) ? s.next_sequence : 1;
}

int64_t OutboxCore::lastAcked() const {
  OutboxState s;
  return loadState(s) ? s.last_acked_sequence : 0;
}

}  // namespace sync
}  // namespace claw4
