// claw4/firmware/tests/fakes/fake_outbox_storage.h
// Deterministic fake outbox storage for host tests.
// Supports failure injection and re-instantiation to simulate power loss /
// process restart. NOT real power-failure safety; never claim NVS-verified.
#pragma once

#include <algorithm>
#include <memory>

#include "sync/outbox_storage.h"

namespace claw4 {
namespace sync {

// Shared "disk" content. A test that wants to simulate a process restart
// simply constructs a NEW FakeOutboxStorage over the SAME FakeDisk.
struct FakeDisk {
  OutboxState state;
  bool fail_next_commit = false;   // injection: next commit fails (nothing written)
  bool fail_next_mark_deadletter = false;  // injection: next dead-letter persist fails
  int commit_calls = 0;
  int diagnostic_calls = 0;
  int ack_calls = 0;
  int deadletter_calls = 0;
  int rebase_calls = 0;
};

class FakeOutboxStorage final : public OutboxStorage {
 public:
  explicit FakeOutboxStorage(std::shared_ptr<FakeDisk> disk) : disk_(std::move(disk)) {}

  bool load(OutboxState& out) override {
    out = disk_->state;
    return true;
  }

  CommitStatus commit(const claw4::domain::DomainState& next_domain,
                      const std::vector<PendingEvent>& appended,
                      int64_t next_sequence) override {
    ++disk_->commit_calls;
    if (disk_->fail_next_commit) {
      disk_->fail_next_commit = false;
      return CommitStatus::StorageError;  // atomic: nothing visible changed
    }
    disk_->state.domain = next_domain;
    for (const auto& row : appended) {
      disk_->state.pending.push_back(row);
    }
    disk_->state.next_sequence = next_sequence;
    return CommitStatus::Committed;
  }

  CommitStatus commitDiagnostic(bool sync_failed, bool sync_recovered) override {
    ++disk_->diagnostic_calls;
    if (disk_->fail_next_commit) {
      disk_->fail_next_commit = false;
      return CommitStatus::StorageError;
    }
    // Mergeable slot: latest state wins.
    disk_->state.diagnostic.sync_failed = sync_failed;
    disk_->state.diagnostic.sync_recovered = sync_recovered;
    return CommitStatus::Committed;
  }

  CommitStatus removeAcked(int64_t up_to_sequence) override {
    ++disk_->ack_calls;
    if (disk_->fail_next_commit) {
      disk_->fail_next_commit = false;
      return CommitStatus::StorageError;
    }
    auto& pending = disk_->state.pending;
    pending.erase(std::remove_if(pending.begin(), pending.end(),
                                 [&](const PendingEvent& p) {
                                   return p.sequence <= up_to_sequence;
                                 }),
                  pending.end());
    disk_->state.last_acked_sequence =
        std::max(disk_->state.last_acked_sequence, up_to_sequence);
    return CommitStatus::Committed;
  }

  CommitStatus markDeadLetter(const claw4::domain::EventId& event_id,
                              const std::string& reason) override {
    ++disk_->deadletter_calls;
    if (disk_->fail_next_mark_deadletter) {
      disk_->fail_next_mark_deadletter = false;
      return CommitStatus::StorageError;  // marker NOT applied, rows retained
    }
    if (disk_->fail_next_commit) {
      disk_->fail_next_commit = false;
      return CommitStatus::StorageError;
    }
    for (auto& p : disk_->state.pending) {
      if (p.event_id == event_id) {
        p.dead_letter_reason = reason;  // original row retained for replay
        break;
      }
    }
    return CommitStatus::Committed;
  }

  // A05-DEVICE-T1: single-commit re-base of the local sequence space onto the
  // server baseline. Every pending row is re-materialized consecutively from
  // new_base + 1, preserving event_id/payload/type/timestamp and their relative
  // order; last_acked_sequence adopts new_base and next_sequence is bumped past
  // the re-numbered rows so later allocations cannot collide.
  CommitStatus rebaseSequences(int64_t new_base) override {
    ++disk_->rebase_calls;
    if (disk_->fail_next_commit) {
      disk_->fail_next_commit = false;
      return CommitStatus::StorageError;  // atomic: nothing visible changed
    }
    int64_t next = new_base;
    std::vector<PendingEvent> rebased;
    rebased.reserve(disk_->state.pending.size());
    for (const auto& p : disk_->state.pending) {
      PendingEvent copy = p;
      copy.sequence = ++next;
      rebased.push_back(copy);
    }
    disk_->state.pending = std::move(rebased);
    disk_->state.last_acked_sequence =
        std::max(disk_->state.last_acked_sequence, new_base);
    if (disk_->state.next_sequence <= next) {
      disk_->state.next_sequence = next + 1;
    }
    return CommitStatus::Committed;
  }

 private:
  std::shared_ptr<FakeDisk> disk_;
};

}  // namespace sync
}  // namespace claw4
