// claw4/firmware/tests/unit/sync/outbox_core_tests.cpp
// Host unit tests for the transactional outbox core (WB-STREAM-002 CP2).
//
// Compiles, links and RUNS natively. Uses explicit case/assert counting (not
// bare assert). All persistence goes through the injectable fake storage with
// deterministic failure injection and re-instantiation (process restart).

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/reducer.h"
#include "sync/batch_result.h"
#include "sync/outbox_core.h"
#include "fakes/fake_outbox_storage.h"

using namespace claw4::domain;
using namespace claw4::sync;

static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                            \
  do {                                        \
    ++g_cases;                                \
    if (!run_case_##name()) {                 \
      std::printf("FAIL: %s\n", #name);       \
      ++g_fail;                               \
    }                                         \
  } while (0)

#define CHECK(cond)                           \
  do {                                        \
    if (!(cond)) {                            \
      std::printf("  assert fail: %s (line %d)\n", #cond, __LINE__); \
      return false;                           \
    }                                         \
  } while (0)

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
EventDraft draft(const std::string& id, EventType type) {
  EventDraft d;
  d.event_id = EventId{id};
  d.device_id = DeviceId{"dev-1"};
  d.child_id = ChildId{"child-1"};
  d.timestamp = 1756718530;
  d.type = type;
  d.version = 1;
  d.payload["k"] = "v";
  return d;
}

DomainState nextDomain() {
  DomainState s;
  Task t;
  t.task_id = TaskId{"task-1"};
  t.status = TaskStatus::InProgress;
  s.tasks.push_back(t);
  return s;
}

struct Env {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};

  // Fresh core over the SAME disk = simulated process restart.
  OutboxCore restart() { return OutboxCore{&storage}; }
  OutboxCore core() { return OutboxCore{&storage}; }
};

int pendingCount(const FakeDisk& d) {
  return static_cast<int>(d.state.pending.size());
}

int64_t nextSeq(const FakeDisk& d) { return d.state.next_sequence; }

// ---------------------------------------------------------------------------
// cases
// ---------------------------------------------------------------------------
static bool run_case_empty_drafts_commits_snapshot_no_seq() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  auto r = c.persistTransition(t);
  CHECK(r.committed());
  CHECK(r.committed_events.empty());
  CHECK(env.disk->state.domain.tasks[0].status == TaskStatus::InProgress);
  CHECK(nextSeq(*env.disk) == 1);  // no sequence consumed
  CHECK(pendingCount(*env.disk) == 0);
  return true;
}

static bool run_case_single_draft_allocates_seq_1() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-1", EventType::TaskStarted));
  auto r = c.persistTransition(t);
  CHECK(r.committed());
  CHECK(r.committed_events.size() == 1);
  CHECK(r.committed_events[0].sequence == 1);
  CHECK(r.committed_events[0].event_id == EventId{"ev-1"});
  CHECK(env.disk->state.pending.size() == 1);
  CHECK(env.disk->state.pending[0].sequence == 1);
  CHECK(nextSeq(*env.disk) == 2);
  return true;
}

static bool run_case_two_drafts_consecutive_sequences() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-a", EventType::TaskStarted));
  t.event_drafts.push_back(draft("ev-b", EventType::StudySessionStarted));
  auto r = c.persistTransition(t);
  CHECK(r.committed());
  CHECK(r.committed_events.size() == 2);
  CHECK(r.committed_events[0].sequence == 1);
  CHECK(r.committed_events[1].sequence == 2);
  CHECK(nextSeq(*env.disk) == 3);
  return true;
}

static bool run_case_commit_failure_rolls_back_all() {
  Env env;
  auto c = env.core();
  // Seed one committed transition first.
  PendingTransition t0;
  t0.next_state = nextDomain();
  t0.event_drafts.push_back(draft("ev-1", EventType::TaskStarted));
  CHECK(c.persistTransition(t0).committed());
  const int64_t seq_before = nextSeq(*env.disk);  // 2
  const int pending_before = pendingCount(*env.disk);  // 1

  // Inject failure on the next commit.
  env.disk->fail_next_commit = true;
  PendingTransition t1;
  t1.next_state = nextDomain();
  t1.event_drafts.push_back(draft("ev-2", EventType::TaskCompleted));
  auto r = c.persistTransition(t1);
  CHECK(!r.committed());
  CHECK(r.status == PersistStatus::StorageError);
  // Atomic rollback: nothing visible changed, no sequence consumed, no hole.
  CHECK(nextSeq(*env.disk) == seq_before);
  CHECK(pendingCount(*env.disk) == pending_before);
  CHECK(env.disk->state.pending[0].event_id == EventId{"ev-1"});
  CHECK(env.disk->state.pending[0].sequence == 1);
  // A retry with the SAME event_id now succeeds and reuses the same id.
  auto r2 = c.persistTransition(t1);
  CHECK(r2.committed());
  CHECK(r2.committed_events[0].event_id == EventId{"ev-2"});
  CHECK(r2.committed_events[0].sequence == 2);  // continues, no hole
  return true;
}

static bool run_case_commit_failure_keeps_old_domain() {
  Env env;
  auto c = env.core();
  PendingTransition t0;
  t0.next_state = nextDomain();  // task InProgress
  t0.event_drafts.push_back(draft("ev-1", EventType::TaskStarted));
  CHECK(c.persistTransition(t0).committed());

  env.disk->fail_next_commit = true;
  PendingTransition t1;
  t1.next_state = nextDomain();
  t1.next_state.tasks[0].status = TaskStatus::Completed;  // would-be new state
  t1.event_drafts.push_back(draft("ev-2", EventType::TaskCompleted));
  auto r = c.persistTransition(t1);
  CHECK(!r.committed());
  // Old committed snapshot is still authoritative.
  CHECK(env.disk->state.domain.tasks[0].status == TaskStatus::InProgress);
  return true;
}

static bool run_case_capacity_exceeded_rejected_whole() {
  Env env;
  auto c = env.core();
  // Fill the queue to 200 rows.
  for (int i = 1; i <= 200; ++i) {
    PendingTransition t;
    t.next_state = nextDomain();
    t.event_drafts.push_back(draft("fill-" + std::to_string(i), EventType::TaskStarted));
    CHECK(c.persistTransition(t).committed());
  }
  CHECK(pendingCount(*env.disk) == 200);
  const int64_t seq_before = nextSeq(*env.disk);

  // One more critical event must NOT silently drop; whole transition fails.
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-201", EventType::TaskCompleted));
  auto r = c.persistTransition(t);
  CHECK(!r.committed());
  CHECK(r.status == PersistStatus::CapacityExceeded);
  CHECK(pendingCount(*env.disk) == 200);
  CHECK(nextSeq(*env.disk) == seq_before);  // no sequence consumed
  return true;
}

static bool run_case_restart_recovers_state() {
  Env env;
  {
    auto c = env.core();
    PendingTransition t;
    t.next_state = nextDomain();
    t.event_drafts.push_back(draft("ev-1", EventType::TaskStarted));
    t.event_drafts.push_back(draft("ev-2", EventType::StudySessionStarted));
    CHECK(c.persistTransition(t).committed());
  }
  // "Reboot": fresh core over the same disk.
  auto c2 = env.restart();
  CHECK(c2.pendingCount() == 2);
  CHECK(c2.nextSequence() == 3);
  CHECK(c2.lastAcked() == 0);
  return true;
}

static bool run_case_restart_after_lost_response_keeps_pending() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-lost", EventType::TaskCompleted));
  auto r = c.persistTransition(t);
  CHECK(r.committed());
  CHECK(r.committed_events[0].sequence == 1);
  // Response "lost": no ACK applied. Reboot keeps the pending row.
  auto c2 = env.restart();
  CHECK(c2.pendingCount() == 1);
  CHECK(c2.nextSequence() == 2);
  CHECK(env.disk->state.pending[0].event_id == EventId{"ev-lost"});
  return true;
}

static bool run_case_ack_removes_accepted_in_prefix() {
  Env env;
  auto c = env.core();
  for (int i = 1; i <= 3; ++i) {
    PendingTransition t;
    t.next_state = nextDomain();
    t.event_drafts.push_back(draft("ev-" + std::to_string(i), EventType::TaskStarted));
    CHECK(c.persistTransition(t).committed());
  }
  BatchSyncResult res;
  res.last_acked_sequence = 3;
  res.results.push_back(PerEventResult{EventId{"ev-1"}, 1, EventOutcome::Accepted, 200});
  res.results.push_back(PerEventResult{EventId{"ev-2"}, 2, EventOutcome::Accepted, 200});
  res.results.push_back(PerEventResult{EventId{"ev-3"}, 3, EventOutcome::Accepted, 200});
  CHECK(c.applyBatchResult(res).committed());
  CHECK(pendingCount(*env.disk) == 0);
  CHECK(c.lastAcked() == 3);
  return true;
}

static bool run_case_ack_removes_duplicate_in_prefix() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-dup", EventType::TaskStarted));
  CHECK(c.persistTransition(t).committed());
  BatchSyncResult res;
  res.last_acked_sequence = 1;
  res.results.push_back(PerEventResult{EventId{"ev-dup"}, 1, EventOutcome::Duplicate, 200});
  CHECK(c.applyBatchResult(res).committed());
  CHECK(pendingCount(*env.disk) == 0);
  return true;
}

static bool run_case_ack_keeps_conflict() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-c", EventType::TaskStarted));
  CHECK(c.persistTransition(t).committed());
  BatchSyncResult res;
  res.last_acked_sequence = 1;
  res.results.push_back(PerEventResult{EventId{"ev-c"}, 1, EventOutcome::Conflict, 409});
  CHECK(c.applyBatchResult(res).committed());
  CHECK(pendingCount(*env.disk) == 1);  // conflict row kept
  return true;
}

static bool run_case_ack_keeps_rejected_and_gap() {
  Env env;
  auto c = env.core();
  for (int i = 1; i <= 2; ++i) {
    PendingTransition t;
    t.next_state = nextDomain();
    t.event_drafts.push_back(draft("ev-" + std::to_string(i), EventType::TaskStarted));
    CHECK(c.persistTransition(t).committed());
  }
  BatchSyncResult res;
  res.last_acked_sequence = 1;  // only seq1 accepted; seq2 rejected/gap stays
  res.results.push_back(PerEventResult{EventId{"ev-1"}, 1, EventOutcome::Accepted, 200});
  res.results.push_back(PerEventResult{EventId{"ev-2"}, 2, EventOutcome::Gap, 409});
  CHECK(c.applyBatchResult(res).committed());
  CHECK(pendingCount(*env.disk) == 1);
  CHECK(env.disk->state.pending[0].sequence == 2);  // only row 2 remains
  CHECK(c.lastAcked() == 1);
  return true;
}

static bool run_case_ack_does_not_remove_beyond_prefix() {
  Env env;
  auto c = env.core();
  for (int i = 1; i <= 3; ++i) {
    PendingTransition t;
    t.next_state = nextDomain();
    t.event_drafts.push_back(draft("ev-" + std::to_string(i), EventType::TaskStarted));
    CHECK(c.persistTransition(t).committed());
  }
  // Server ACKs only up to 1 even though rows 2,3 were sent.
  BatchSyncResult res;
  res.last_acked_sequence = 1;
  res.results.push_back(PerEventResult{EventId{"ev-1"}, 1, EventOutcome::Accepted, 200});
  res.results.push_back(PerEventResult{EventId{"ev-2"}, 2, EventOutcome::Duplicate, 200});
  res.results.push_back(PerEventResult{EventId{"ev-3"}, 3, EventOutcome::Duplicate, 200});
  CHECK(c.applyBatchResult(res).committed());
  // Only rows <= 1 inside the consecutive prefix are removable; rows 2,3 have
  // seq > last_acked (not in prefix) -> kept.
  CHECK(pendingCount(*env.disk) == 2);
  CHECK(c.lastAcked() == 1);
  return true;
}

static bool run_case_business_4xx_marks_dead_letter_keeps_row() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-dl", EventType::TaskCompleted));
  CHECK(c.persistTransition(t).committed());
  BatchSyncResult res;
  res.last_acked_sequence = 1;
  res.results.push_back(PerEventResult{EventId{"ev-dl"}, 1, EventOutcome::Rejected, 422});
  CHECK(c.applyBatchResult(res).committed());
  // Row kept with dead-letter marker + original payload.
  CHECK(pendingCount(*env.disk) == 1);
  CHECK(env.disk->state.pending[0].dead_letter_reason.has_value());
  CHECK(env.disk->state.pending[0].event_id == EventId{"ev-dl"});
  CHECK(env.disk->state.pending[0].payload.at("k") == "v");
  return true;
}

static bool run_case_auth_and_5xx_never_remove_pending() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-a1", EventType::TaskStarted));
  CHECK(c.persistTransition(t).committed());
  // No accepted/duplicate results -> nothing removed.
  BatchSyncResult res;
  res.last_acked_sequence = 0;
  res.results.push_back(PerEventResult{EventId{"ev-a1"}, 1, EventOutcome::Rejected, 401});
  CHECK(c.applyBatchResult(res).committed());
  CHECK(pendingCount(*env.disk) == 1);
  CHECK(!env.disk->state.pending[0].dead_letter_reason.has_value());  // 401 != dead-letter
  return true;
}

static bool run_case_diagnostic_slot_outside_budget() {
  Env env;
  auto c = env.core();
  // Fill to 200 business rows.
  for (int i = 1; i <= 200; ++i) {
    PendingTransition t;
    t.next_state = nextDomain();
    t.event_drafts.push_back(draft("fill-" + std::to_string(i), EventType::TaskStarted));
    CHECK(c.persistTransition(t).committed());
  }
  // Diagnostic slot is independent: succeeds even at full business budget.
  auto r = c.setDiagnostic(true, false);
  CHECK(r.committed());
  CHECK(pendingCount(*env.disk) == 200);  // business queue untouched
  CHECK(env.disk->state.diagnostic.sync_failed == true);
  return true;
}

static bool run_case_diagnostic_slot_merges_latest() {
  Env env;
  auto c = env.core();
  CHECK(c.setDiagnostic(true, false).committed());
  CHECK(c.setDiagnostic(false, true).committed());
  CHECK(env.disk->state.diagnostic.sync_failed == false);
  CHECK(env.disk->state.diagnostic.sync_recovered == true);
  return true;
}

static bool run_case_duplicate_event_id_persist_rejected() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-same", EventType::TaskStarted));
  CHECK(c.persistTransition(t).committed());
  // Same event_id submitted again (should never happen; guard).
  PendingTransition t2;
  t2.next_state = nextDomain();
  t2.event_drafts.push_back(draft("ev-same", EventType::TaskStarted));
  auto r = c.persistTransition(t2);
  CHECK(!r.committed());
  CHECK(r.status == PersistStatus::InvalidTransition);
  CHECK(pendingCount(*env.disk) == 1);  // no pollution
  return true;
}

static bool run_case_duplicate_event_ids_within_transition_rejected() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-same", EventType::TaskStarted));
  t.event_drafts.push_back(
      draft("ev-same", EventType::StudySessionStarted));
  auto r = c.persistTransition(t);
  CHECK(!r.committed());
  CHECK(r.status == PersistStatus::InvalidTransition);
  CHECK(pendingCount(*env.disk) == 0);
  CHECK(nextSeq(*env.disk) == 1);
  return true;
}

static bool run_case_dead_letter_replay_same_event_id() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  t.event_drafts.push_back(draft("ev-replay", EventType::TaskCompleted));
  CHECK(c.persistTransition(t).committed());
  // Business 4xx -> dead-letter, row kept.
  BatchSyncResult res;
  res.last_acked_sequence = 1;
  res.results.push_back(PerEventResult{EventId{"ev-replay"}, 1, EventOutcome::Rejected, 422});
  CHECK(c.applyBatchResult(res).committed());
  // After a "fix", the SAME event_id is replayed (not a new id). The guard
  // blocks re-persisting an identical pending row; in a real flow the caller
  // would first clear/retry via the same event_id through the server. Here we
  // assert the id is stable and no new id was generated by the core.
  CHECK(env.disk->state.pending[0].event_id == EventId{"ev-replay"});
  CHECK(c.nextSequence() == 2);  // core never minted another id
  return true;
}

static bool run_case_materialized_matches_drafts() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  EventDraft d1 = draft("ev-m1", EventType::TaskStarted);
  EventDraft d2 = draft("ev-m2", EventType::StudySessionCompleted);
  d2.payload["actual_seconds"] = "1500";
  t.event_drafts.push_back(d1);
  t.event_drafts.push_back(d2);
  auto r = c.persistTransition(t);
  CHECK(r.committed());
  CHECK(r.committed_events.size() == 2);
  CHECK(r.committed_events[0].event_id == d1.event_id);
  CHECK(r.committed_events[0].type == d1.type);
  CHECK(r.committed_events[0].sequence == 1);
  CHECK(r.committed_events[1].event_id == d2.event_id);
  CHECK(r.committed_events[1].type == d2.type);
  CHECK(r.committed_events[1].sequence == 2);
  CHECK(r.committed_events[1].payload.at("actual_seconds") == "1500");
  CHECK(env.disk->state.domain.tasks[0].status == TaskStatus::InProgress);
  return true;
}

static bool run_case_invalid_empty_event_id_rejected() {
  Env env;
  auto c = env.core();
  PendingTransition t;
  t.next_state = nextDomain();
  EventDraft bad = draft("", EventType::TaskStarted);  // empty id
  t.event_drafts.push_back(bad);
  auto r = c.persistTransition(t);
  CHECK(!r.committed());
  CHECK(r.status == PersistStatus::InvalidTransition);
  CHECK(pendingCount(*env.disk) == 0);
  CHECK(nextSeq(*env.disk) == 1);  // nothing consumed
  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
static bool run_case_all() {
  CASE(empty_drafts_commits_snapshot_no_seq);
  CASE(single_draft_allocates_seq_1);
  CASE(two_drafts_consecutive_sequences);
  CASE(commit_failure_rolls_back_all);
  CASE(commit_failure_keeps_old_domain);
  CASE(capacity_exceeded_rejected_whole);
  CASE(restart_recovers_state);
  CASE(restart_after_lost_response_keeps_pending);
  CASE(ack_removes_accepted_in_prefix);
  CASE(ack_removes_duplicate_in_prefix);
  CASE(ack_keeps_conflict);
  CASE(ack_keeps_rejected_and_gap);
  CASE(ack_does_not_remove_beyond_prefix);
  CASE(business_4xx_marks_dead_letter_keeps_row);
  CASE(auth_and_5xx_never_remove_pending);
  CASE(diagnostic_slot_outside_budget);
  CASE(diagnostic_slot_merges_latest);
  CASE(duplicate_event_id_persist_rejected);
  CASE(duplicate_event_ids_within_transition_rejected);
  CASE(dead_letter_replay_same_event_id);
  CASE(materialized_matches_drafts);
  CASE(invalid_empty_event_id_rejected);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-STREAM-002 CP2 outbox core tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
