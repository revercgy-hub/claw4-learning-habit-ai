// claw4/integration/metalio_claw4/device/ports/nvs_outbox_storage.h
// WB-LEARNING-V4-L1 — OutboxStorage over the NVS partition, namespace
// "learning", single key "st" (encoded by outbox_codec).
//
// Commit semantics mirror the host FakeOutboxStorage (load -> mutate -> save):
// on any NVS/codec failure nothing is written, so the previously committed
// snapshot stays visible (the coordinator maps failures to StorageError and
// the transition is never published).
#pragma once

#include <string>

#include "sync/outbox_storage.h"

namespace claw4 {
namespace metalio {

class NvsOutboxStorage final : public claw4::sync::OutboxStorage {
 public:
  enum class StateReadStatus { Missing, Present, Error };
  NvsOutboxStorage() = default;
  ~NvsOutboxStorage() override = default;

  bool load(claw4::sync::OutboxState& out) override;
  // Single-read presence probe for boot policy; distinguishes an absent blob
  // from a read/decode failure without a second NVS lookup.
  StateReadStatus loadWithPresence(claw4::sync::OutboxState& out);

  claw4::sync::CommitStatus commit(
      const claw4::domain::DomainState& next_domain,
      const std::vector<claw4::sync::PendingEvent>& appended,
      int64_t next_sequence) override;

  claw4::sync::CommitStatus commitDiagnostic(bool sync_failed,
                                             bool sync_recovered) override;

  claw4::sync::CommitStatus removeAcked(int64_t up_to_sequence) override;

  claw4::sync::CommitStatus markDeadLetter(
      const claw4::domain::EventId& event_id,
      const std::string& reason) override;

  // Factory/erasure helpers for boot-time seeding & tests.
  static bool eraseAll();  // removes the learning namespace state (demo reset)
  static bool hasState();  // whether a committed blob exists

 private:
  claw4::sync::CommitStatus Save(claw4::sync::OutboxState cur);
};

}  // namespace metalio
}  // namespace claw4
