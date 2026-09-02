// claw4/firmware/tests/unit/application/coordinator_tests.cpp
// Host unit tests for the application coordinator (WB-STREAM-002 CP3).
//
// Verifies the commit-then-publish pipeline, incremental today-task cache
// merging, consecutive-prefix sync batching, single re-auth, deterministic
// backoff and per-event retention. All transports/ids/times are injected; the
// tests never network, sleep or use RNG.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "application/coordinator.h"
#include "learning_domain/reducer.h"
#include "fakes/fake_outbox_storage.h"
#include "fakes/fake_sync_transport.h"

using namespace claw4::domain;
using namespace claw4::sync;
using namespace claw4::application;
using namespace claw4::fakes;

static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                             \
  do {                                         \
    ++g_cases;                                 \
    if (!run_case_##name()) {                  \
      std::printf("FAIL: %s\n", #name);        \
      ++g_fail;                                \
    }                                          \
  } while (0)

#define CHECK(cond)                            \
  do {                                         \
    if (!(cond)) {                             \
      std::printf("  assert fail: %s (line %d)\n", #cond, __LINE__); \
      return false;                            \
    }                                          \
  } while (0)

// ---------------------------------------------------------------------------
// environment
// ---------------------------------------------------------------------------
struct CtxGen {
  DeviceId device{"dev-1"};
  ChildId child{"child-1"};
  int64_t epoch = 1756718530;
  int64_t mono = 1'000'000;
  int e = 0;
  int s = 0;

  ReducerContext make() {
    ReducerContext c;
    c.device_id = device;
    c.child_id = child;
    c.now_epoch = epoch;
    c.monotonic_ms = mono;
    c.make_event_id = [this]() { return EventId{"ev-" + std::to_string(++e)}; };
    c.make_session_id = [this]() { return SessionId{"sess-" + std::to_string(++s)}; };
    return c;
  }
  void advance(int64_t ms) { mono += ms; }
};

struct Env {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  CoordinatorOptions opts;
  CtxGen ctx;

  AppCoordinator make() {
    AppCoordinator c{storage, reducer, opts};
    return c;
  }
};

Task readyTask(const std::string& id, int version) {
  Task t;
  t.task_id = TaskId{id};
  t.child_id = ChildId{"child-1"};
  t.title = "task-" + id;
  t.status = TaskStatus::Ready;
  t.version = version;
  return t;
}

IntentRequest startOf(const std::string& tid = "task-1") {
  IntentRequest r;
  r.intent = Intent::StartTask;
  r.task_id = TaskId{tid};
  return r;
}

// Seed one ready task into the committed domain.
bool seedTask(Env& env, const Task& t) {
  auto c = env.make();
  return c.mergeTodayTasks({t});
}

// ---------------------------------------------------------------------------
// cases
// ---------------------------------------------------------------------------
static bool run_case_dispatch_start_publishes_after_commit() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  const auto r = c.dispatchIntent(startOf(), env.ctx.make());
  CHECK(r.ok);
  CHECK(c.state().tasks[0].status == TaskStatus::InProgress);   // published
  CHECK(c.state().active_session.has_value());
  CHECK(c.pendingCount() == 2);                                  // committed events
  return true;
}

static bool run_case_dispatch_reducer_reject_not_published() {
  Env env;
  Task done = readyTask("task-1", 3);
  done.status = TaskStatus::Completed;
  CHECK(seedTask(env, done));
  auto c = env.make();
  const auto r = c.dispatchIntent(startOf(), env.ctx.make());
  CHECK(!r.ok);
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);  // untouched
  CHECK(c.pendingCount() == 0);
  return true;
}

static bool run_case_dispatch_outbox_failure_not_published() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  env.disk->fail_next_commit = true;
  const auto r = c.dispatchIntent(startOf(), env.ctx.make());
  CHECK(!r.ok);
  CHECK(r.intent_result == IntentResult::PersistFailed);
  CHECK(c.state().tasks[0].status == TaskStatus::Ready);  // not published
  CHECK(c.pendingCount() == 0);
  return true;
}

static bool run_case_dispatch_timer_timeout_snapshot_commit() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  auto s1 = c.dispatchIntent(startOf(), env.ctx.make());
  CHECK(s1.ok);
  env.ctx.advance(25 * 60'000);
  const auto to = c.dispatchSegmentTimeout(s1.next->active_session->session_id,
                                           env.ctx.make());
  CHECK(to.ok);
  CHECK(c.state().tasks[0].status == TaskStatus::Paused);  // not Completed
  CHECK(c.state().active_session->status == SessionStatus::Paused);
  CHECK(c.pendingCount() == 2);  // snapshot-only commit added no events
  return true;
}

static bool run_case_merge_tasks_incremental_version() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 1)));
  auto c = env.make();
  // Older version ignored.
  CHECK(c.mergeTodayTasks({readyTask("task-1", 0)}));
  CHECK(c.state().tasks[0].version == 1);
  // Newer version applied.
  CHECK(c.mergeTodayTasks({readyTask("task-1", 5)}));
  CHECK(c.state().tasks[0].version == 5);
  // New task appended.
  CHECK(c.mergeTodayTasks({readyTask("task-2", 1)}));
  CHECK(c.state().tasks.size() == 2);
  return true;
}

static bool run_case_merge_keeps_running_task_status() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  const auto s1 = c.dispatchIntent(startOf(), env.ctx.make());  // task InProgress
  CHECK(s1.ok);
  // Server sends an updated version with status=Ready while task is running:
  // the local live status must NOT be overwritten.
  Task updated = readyTask("task-1", 9);
  updated.status = TaskStatus::Ready;
  CHECK(c.mergeTodayTasks({updated}));
  CHECK(c.state().tasks[0].version == 9);
  CHECK(c.state().tasks[0].status == TaskStatus::InProgress);  // live status kept
  CHECK(c.state().active_session.has_value());                 // session intact
  return true;
}

static bool run_case_merge_empty_tasks_ok() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.mergeTodayTasks({}));  // empty list is a normal state
  CHECK(c.state().tasks.size() == 1);  // merge does not wipe the local cache
  return true;
}

static bool run_case_merge_persists_across_reboot() {
  Env env;
  {
    auto c = env.make();
    CHECK(c.mergeTodayTasks({readyTask("task-1", 3)}));
  }
  auto c2 = env.make();  // "reboot"
  CHECK(c2.state().tasks.size() == 1);
  CHECK(c2.state().tasks[0].task_id == TaskId{"task-1"});
  return true;
}

static bool run_case_sync_sends_consecutive_prefix_only() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // 2 pending (seq 1,2)
  env.ctx.advance(60'000);
  CHECK(c.dispatchIntent([&] {
    IntentRequest r;
    r.intent = Intent::Complete;
    r.task_id = TaskId{"task-1"};
    return r;
  }(), env.ctx.make()).ok);  // +2 pending (seq 3,4)
  CHECK(c.pendingCount() == 4);

  FakeSyncTransport tr;
  // Only the prefix starting at last_acked+1 is sent. Everything is pending and
  // consecutive -> all 4 are sent in one batch.
  const auto oc = c.runSyncOnce(tr, [] { return true; });
  CHECK(oc == SyncOutcome::Synced);
  CHECK(tr.requests.size() == 1);
  CHECK(tr.requests[0].events.size() == 4);
  CHECK(tr.requests[0].events[0].sequence == 1);
  CHECK(tr.requests[0].events[3].sequence == 4);
  return true;
}

static bool run_case_sync_cleans_pending_on_success() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  CHECK(c.pendingCount() == 2);
  FakeSyncTransport tr;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);  // Accepted inside prefix -> removed
  CHECK(c.lastAcked() == 2);
  return true;
}

static bool run_case_sync_duplicate_response_converges() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  // Simulate a lost response: no ACK applied, pending remains. On the retry
  // the server reports Duplicate for everything (idempotent convergence).
  FakeSyncTransport tr;
  tr.on_send = [&tr]() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    const auto& req = tr.requests.back();
    r.batch.last_acked_sequence = req.last_acked_sequence;
    for (const auto& e : req.events) {
      PerEventResult per;
      per.event_id = e.event_id;
      per.sequence = e.sequence;
      per.outcome = EventOutcome::Duplicate;  // server already persisted them
      per.http_status = 200;
      r.batch.results.push_back(per);
      r.batch.last_acked_sequence = e.sequence;
    }
    return r;
  };
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);  // Duplicate inside prefix -> cleaned
  CHECK(c.lastAcked() == 2);
  return true;
}

static bool run_case_sync_auth_reauth_ok_once() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  FakeSyncTransport tr;
  tr.on_send = []() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::Auth;
    r.http_status = 401;
    return r;
  };
  int reauth_calls = 0;
  const auto oc = c.runSyncOnce(tr, [&] { ++reauth_calls; return true; });
  CHECK(oc == SyncOutcome::ReauthOk);
  CHECK(reauth_calls == 1);
  CHECK(c.pendingCount() == 2);  // pending untouched by auth failure
  return true;
}

static bool run_case_sync_auth_reauth_fail_pauses() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  FakeSyncTransport tr;
  tr.on_send = []() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::Auth;
    r.http_status = 403;
    return r;
  };
  int reauth_calls = 0;
  CHECK(c.runSyncOnce(tr, [&] { ++reauth_calls; return false; }) ==
        SyncOutcome::PausedAuth);
  CHECK(reauth_calls == 1);
  CHECK(c.pendingCount() == 2);  // pending untouched
  // Second attempt: re-auth already attempted -> paused without calling again.
  CHECK(c.runSyncOnce(tr, [&] { ++reauth_calls; return false; }) ==
        SyncOutcome::PausedAuth);
  CHECK(reauth_calls == 1);  // no second re-auth attempt
  return true;
}

static bool run_case_sync_network_backoff_grows_to_cap() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // 2 pending
  FakeSyncTransport tr;
  tr.on_send = []() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::Network;
    r.http_status = 502;
    return r;
  };
  // Deterministic: base=1000ms doubling, cap 60000ms, zero jitter.
  env.opts.backoff_base_ms = 1000;
  env.opts.backoff_max_ms = 60000;
  env.opts.jitter_amplitude_ms = 0;
  auto c2 = env.make();  // re-load committed state (task InProgress, 2 pending)
  CHECK(c2.pendingCount() == 2);
  CHECK(c2.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Backoff);
  CHECK(c2.backoffDelayMs() == 1000);
  CHECK(c2.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Backoff);
  CHECK(c2.backoffDelayMs() == 2000);
  // Keep failing until the cap (60 s) is reached.
  for (int i = 0; i < 20; ++i) {
    c2.runSyncOnce(tr, [] { return true; });
  }
  CHECK(c2.backoffDelayMs() <= 60000);
  CHECK(c2.pendingCount() == 2);  // never deleted on network errors
  return true;
}

static bool run_case_sync_business_4xx_deadletter_keeps_pending() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  FakeSyncTransport tr;
  tr.on_send = [&tr]() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    const auto& req = tr.requests.back();
    r.batch.last_acked_sequence = req.last_acked_sequence;
    for (const auto& e : req.events) {
      PerEventResult per;
      per.event_id = e.event_id;
      per.sequence = e.sequence;
      per.outcome = EventOutcome::Rejected;  // business rejection
      per.http_status = 422;
      r.batch.results.push_back(per);
    }
    return r;
  };
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 2);  // kept (dead-lettered, replayable)
  CHECK(env.disk->state.pending[0].dead_letter_reason.has_value());
  CHECK(env.disk->state.pending[0].event_id == EventId{"ev-1"});
  return true;
}

static bool run_case_sync_conflict_keeps_pending_no_cleanup() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  FakeSyncTransport tr;
  tr.on_send = [&tr]() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    const auto& req = tr.requests.back();
    r.batch.last_acked_sequence = req.last_acked_sequence;
    for (const auto& e : req.events) {
      PerEventResult per;
      per.event_id = e.event_id;
      per.sequence = e.sequence;
      per.outcome = EventOutcome::Conflict;
      per.http_status = 409;
      r.batch.results.push_back(per);
    }
    return r;
  };
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 2);  // conflict rows kept, no ACK advance
  CHECK(c.lastAcked() == 0);
  return true;
}

static bool run_case_lost_response_then_duplicate_converges() {
  Env env;
  Task t = readyTask("task-1", 3);
  {
    auto c = env.make();
    CHECK(c.mergeTodayTasks({t}));
    // Commit the events but "lose" the response: never sync.
    CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  }
  // Reboot.
  auto c2 = env.make();
  CHECK(c2.pendingCount() == 2);
  CHECK(c2.state().tasks[0].status == TaskStatus::InProgress);  // committed state
  FakeSyncTransport tr2;
  tr2.on_send = [&tr2]() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    const auto& req = tr2.requests.back();
    r.batch.last_acked_sequence = req.last_acked_sequence;
    for (const auto& e : req.events) {
      PerEventResult per;
      per.event_id = e.event_id;
      per.sequence = e.sequence;
      per.outcome = EventOutcome::Duplicate;  // server persisted earlier
      per.http_status = 200;
      r.batch.results.push_back(per);
      r.batch.last_acked_sequence = e.sequence;
    }
    return r;
  };
  CHECK(c2.runSyncOnce(tr2, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c2.pendingCount() == 0);  // converged after reboot + duplicate
  return true;
}

static bool run_case_sync_diagnostic_slot_set_on_success() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  FakeSyncTransport tr;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(env.disk->state.diagnostic.sync_failed == false);
  CHECK(env.disk->state.diagnostic.sync_recovered == true);
  return true;
}

static bool run_case_end_to_end_start_complete_sync() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  // Start -> 2 pending; Complete -> +2 pending; sync -> all accepted.
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  env.ctx.advance(1500'000);
  IntentRequest comp;
  comp.intent = Intent::Complete;
  comp.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(comp, env.ctx.make()).ok);
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);
  CHECK(c.pendingCount() == 4);
  FakeSyncTransport tr;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);
  CHECK(c.lastAcked() == 4);
  return true;
}

static bool run_case_dispatch_retry_same_event_id_after_outbox_failure() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  env.disk->fail_next_commit = true;
  const auto r1 = c.dispatchIntent(startOf(), env.ctx.make());  // ev-1, ev-2
  CHECK(!r1.ok);
  CHECK(r1.intent_result == IntentResult::PersistFailed);
  CHECK(c.pendingCount() == 0);
  // Retry reuses the SAME event_id values (no new ids are minted).
  const auto r2 = c.retryLastFailed();
  CHECK(r2.ok);
  CHECK(c.pendingCount() == 2);
  CHECK(env.disk->state.pending[0].event_id == EventId{"ev-1"});
  CHECK(env.disk->state.pending[1].event_id == EventId{"ev-2"});
  CHECK(c.state().tasks[0].status == TaskStatus::InProgress);  // published
  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
static bool run_case_all() {
  CASE(dispatch_start_publishes_after_commit);
  CASE(dispatch_reducer_reject_not_published);
  CASE(dispatch_outbox_failure_not_published);
  CASE(dispatch_timer_timeout_snapshot_commit);
  CASE(merge_tasks_incremental_version);
  CASE(merge_keeps_running_task_status);
  CASE(merge_empty_tasks_ok);
  CASE(merge_persists_across_reboot);
  CASE(sync_sends_consecutive_prefix_only);
  CASE(sync_cleans_pending_on_success);
  CASE(sync_duplicate_response_converges);
  CASE(sync_auth_reauth_ok_once);
  CASE(sync_auth_reauth_fail_pauses);
  CASE(sync_network_backoff_grows_to_cap);
  CASE(sync_business_4xx_deadletter_keeps_pending);
  CASE(sync_conflict_keeps_pending_no_cleanup);
  CASE(lost_response_then_duplicate_converges);
  CASE(sync_diagnostic_slot_set_on_success);
  CASE(end_to_end_start_complete_sync);
  CASE(dispatch_retry_same_event_id_after_outbox_failure);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-STREAM-002 CP3 application coordinator tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
