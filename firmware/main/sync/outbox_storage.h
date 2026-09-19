// claw4/firmware/main/sync/outbox_storage.h
// Injectable persistence interface for the transactional outbox core.
// Portable C++17. No hardware dependencies. Real NVS/filesystem adapters are
// out of scope for this stream (WB-STREAM-002 CP2).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"
#include "learning_domain/ids.h"

namespace claw4 {
namespace sync {

// A fully materialized pending event with the sequence allocated by the outbox
// transaction (the ONLY sequence owner on the device).
struct PendingEvent {
  claw4::domain::EventId event_id;
  claw4::domain::DeviceId device_id;
  claw4::domain::ChildId child_id;
  int64_t sequence = 0;
  int64_t timestamp = 0;
  claw4::domain::TimestampSource timestamp_source = claw4::domain::TimestampSource::Local;
  claw4::domain::EventType type = claw4::domain::EventType::DeviceBooted;
  int version = 1;
  std::map<std::string, std::string> payload;
  // Business 4xx dead-letter marker: the original row is KEPT (same event_id
  // and original payload) so it can be replayed after a fix; reason is a
  // short redacted summary. Not set for auth/network/5xx outcomes.
  std::optional<std::string> dead_letter_reason;
};

// Mergeable local diagnostic slot for sync.failed / sync.recovered. It NEVER
// enters the business pending queue and does NOT consume the 200-row budget.
struct DiagnosticSlot {
  bool sync_failed = false;     // last merged state
  bool sync_recovered = false;  // last merged state
};

// The full persisted outbox state (domain snapshot + pending + counters).
struct OutboxState {
  claw4::domain::DomainState domain;
  std::vector<PendingEvent> pending;   // ordered by sequence, <= 200 rows
  int64_t next_sequence = 1;           // next sequence to allocate
  int64_t last_acked_sequence = 0;     // consecutive prefix known to be ACKed
  DiagnosticSlot diagnostic;
};

// Result of a commit.
enum class CommitStatus : uint8_t {
  Committed = 0,
  StorageError,
};

// Abstract persistence. Implementations (fake for host tests; NVS adapter in a
// later hardware stream) must guarantee that commit() is atomic: on failure
// the previous snapshot, pending rows and sequence counter all stay visible.
class OutboxStorage {
 public:
  virtual ~OutboxStorage() = default;

  // Loads the current persisted state (used at boot / after re-instantiation).
  virtual bool load(OutboxState& out) = 0;

  // Atomically persists: new domain snapshot, appended pending rows (already
  // sequence-materialized by the core) and the advanced next_sequence.
  virtual CommitStatus commit(const claw4::domain::DomainState& next_domain,
                              const std::vector<PendingEvent>& appended,
                              int64_t next_sequence) = 0;

  // Updates the mergeable diagnostic slot (sync.failed / sync.recovered).
  virtual CommitStatus commitDiagnostic(bool sync_failed, bool sync_recovered) = 0;

  // Removes pending rows whose sequence <= up_to_sequence (only called after
  // the caller confirmed Accepted/Duplicate results inside the consecutive
  // ACK prefix — see OutboxCore::applyAck).
  virtual CommitStatus removeAcked(int64_t up_to_sequence) = 0;

  // Marks a single pending row as dead-lettered (business 4xx). The row is
  // KEPT with its original event_id/payload; only the marker changes.
  virtual CommitStatus markDeadLetter(const claw4::domain::EventId& event_id,
                                      const std::string& reason) = 0;

  // A05-DEVICE-T1 (ACK_PIPELINE_FAIL): re-bases the LOCAL sequence space onto
  // the baseline the server announced (`new_base`).
  //
  // Every pending row whose sequence <= new_base is re-materialized at a fresh
  // consecutive sequence above new_base, keeping its event_id, payload, type and
  // timestamp; the superseded rows are then dropped and last_acked_sequence
  // becomes new_base. next_sequence is left consistent so the next
  // prepareSync() starts at new_base + 1.
  //
  // Why: removeAcked() is a LOW-WATER-MARK delete, so a single un-ackable row
  // at the head of the queue blocks the removal of every row behind it forever
  // when the device's sequence space and the server's have diverged. Re-basing
  // is the only lossless way out.
  //
  // Implementations MUST commit this atomically (one commit): a crash must not
  // be able to lose a row. Implementations that cannot do so MUST NOT silently
  // succeed. Default: unsupported -> StorageError (fail-closed), which makes the
  // coordinator report a blocked queue instead of pretending to be healthy.
  virtual CommitStatus rebaseSequences(int64_t new_base) {
    (void)new_base;
    return CommitStatus::StorageError;
  }
};

}  // namespace sync
}  // namespace claw4
