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
  return c.applyTodaySnapshot({t});
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

static bool run_case_snapshot_version_guard_and_append() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 5)));
  auto c = env.make();
  // Older server version (stale replica) must not regress the local row.
  CHECK(c.applyTodaySnapshot({readyTask("task-1", 3)}));
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].version == 5);
  // Newer version applied.
  CHECK(c.applyTodaySnapshot({readyTask("task-1", 7)}));
  CHECK(c.state().tasks[0].version == 7);
  // Server always sends the full today list: adding a new task appends.
  CHECK(c.applyTodaySnapshot({readyTask("task-1", 7), readyTask("task-2", 1)}));
  CHECK(c.state().tasks.size() == 2);
  return true;
}

static bool run_case_snapshot_a_b_to_a_removes_b() {
  Env env;
  auto c = env.make();
  CHECK(c.applyTodaySnapshot({readyTask("task-a", 1), readyTask("task-b", 1)}));
  CHECK(c.state().tasks.size() == 2);
  // Next authoritative snapshot no longer lists B -> B is removed.
  CHECK(c.applyTodaySnapshot({readyTask("task-a", 1)}));
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].task_id == TaskId{"task-a"});
  return true;
}

static bool run_case_snapshot_keeps_running_task_status() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  const auto s1 = c.dispatchIntent(startOf(), env.ctx.make());  // task InProgress
  CHECK(s1.ok);
  // Server sends an updated version with status=Ready while task is running:
  // the local live status must NOT be overwritten.
  Task updated = readyTask("task-1", 9);
  updated.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({updated}));
  CHECK(c.state().tasks[0].version == 9);
  CHECK(c.state().tasks[0].status == TaskStatus::InProgress);  // live status kept
  CHECK(c.state().active_session.has_value());                 // session intact
  return true;
}

static bool run_case_snapshot_active_retained_when_server_empty() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // Running
  // server=[]: the live task/session must NOT be destroyed by the snapshot.
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].status == TaskStatus::InProgress);
  CHECK(c.state().active_session.has_value());
  CHECK(c.pendingCount() == 2);  // session events intact
  return true;
}

static bool run_case_snapshot_empty_removes_non_active_tasks() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  // server=[] is an authoritative "no tasks today": non-active rows removed.
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.empty());
  return true;
}

static bool run_case_snapshot_cleans_completed_after_terminal_acked() {
  // REVIEW-FIX-001 RF3 changed the frozen semantics: while the terminal event
  // is UN-ACKed the local row is a tombstone and must survive an empty
  // snapshot (see run_case_a04_complete_empty_snapshot_stale_ready_not_revived).
  // Once the terminal event is ACKed the server is authoritative again, so a
  // snapshot that no longer lists the task removes it. This case keeps its
  // original purpose (completed rows ARE cleaned up) under the new rule.
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // Running
  env.ctx.advance(60'000);
  IntentRequest comp;
  comp.intent = Intent::Complete;
  comp.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(comp, env.ctx.make()).ok);  // Completed, session cleared
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);
  // Confirm the terminal events with the server first.
  FakeSyncTransport tr;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);
  // Now the authoritative snapshot (server no longer lists it) may remove it.
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.empty());
  return true;
}

static bool run_case_snapshot_persists_across_reboot() {
  Env env;
  {
    auto c = env.make();
    CHECK(c.applyTodaySnapshot({readyTask("task-1", 3)}));
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
  CHECK(!c.authPaused());        // re-auth succeeded: not paused yet
  // Once the credential is valid the very next attempt sends normally.
  tr.on_send = []() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    return r;
  };
  CHECK(c.runSyncOnce(tr, [&] { ++reauth_calls; return true; }) ==
        SyncOutcome::Synced);
  CHECK(reauth_calls == 1);      // no further re-auth on success
  CHECK(tr.requests.size() == 2);
  CHECK(c.pendingCount() == 0);
  CHECK(c.lastAcked() == 2);
  CHECK(!c.authPaused());
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
  CHECK(c.authPaused());
  // Second attempt: auth-pause gate short-circuits — no send, no re-auth.
  CHECK(c.runSyncOnce(tr, [&] { ++reauth_calls; return false; }) ==
        SyncOutcome::PausedAuth);
  CHECK(reauth_calls == 1);        // no second re-auth attempt
  CHECK(tr.requests.size() == 1);  // FIX-V4-01: transport NOT contacted again
  return true;
}

static bool run_case_sync_auth_pause_stops_sending_until_reset() {
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
  CHECK(c.runSyncOnce(tr, [&] { ++reauth_calls; return false; }) ==
        SyncOutcome::PausedAuth);
  CHECK(c.authPaused());
  const int sends_at_pause = static_cast<int>(tr.requests.size());
  CHECK(sends_at_pause == 1);
  // While paused: 3 more attempts must not touch the transport, re-auth or
  // pending at all (FIX-V4-01).
  for (int i = 0; i < 3; ++i) {
    CHECK(c.runSyncOnce(tr, [&] { ++reauth_calls; return false; }) ==
          SyncOutcome::PausedAuth);
  }
  CHECK(tr.requests.size() == 1);  // send count unchanged
  CHECK(reauth_calls == 1);        // re-auth not called again
  CHECK(c.pendingCount() == 2);    // pending untouched
  CHECK(c.lastAcked() == 0);
  // Explicit recovery resumes sending.
  c.resetAuthPause();
  CHECK(!c.authPaused());
  tr.on_send = []() -> SyncClient::Response {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    return r;
  };
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(tr.requests.size() == 2);
  CHECK(c.pendingCount() == 0);  // events were never lost across the pause
  CHECK(c.lastAcked() == 2);
  CHECK(!c.authPaused());
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
  // A05-DEVICE-T1: the batch WAS applied (rows dead-lettered) but the queue did
  // not move, so the cycle is reported as Blocked instead of a false Synced.
  // The rows are still kept and replayable, exactly as before.
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Blocked);
  CHECK(c.pendingCount() == 2);  // kept (dead-lettered, replayable)
  CHECK(env.disk->state.pending[0].dead_letter_reason.has_value());
  CHECK(env.disk->state.pending[0].event_id == EventId{"ev-1"});
  return true;
}

static bool run_case_sync_deadletter_storage_failure_blocks_cleanup() {
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
  // Dead-letter persistence fails: whole batch application must abort
  // (FIX-V4-03) -> Backoff; pending kept; ACK not advanced; no cleanup.
  env.disk->fail_next_mark_deadletter = true;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Backoff);
  CHECK(c.pendingCount() == 2);
  CHECK(c.lastAcked() == 0);
  CHECK(env.disk->ack_calls == 0);  // removeAcked never called
  CHECK(!env.disk->state.pending[0].dead_letter_reason.has_value());
  // Storage recovers: the same business 422s now dead-letter cleanly, but the
  // queue still cannot advance (the dead-lettered rows block the prefix), so the
  // honest outcome is Blocked — see A05-DEVICE-T1.
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Blocked);
  CHECK(c.pendingCount() == 2);  // dead-lettered rows stay replayable
  CHECK(env.disk->state.pending[0].dead_letter_reason.has_value());
  // 1 failed attempt (storage error) + 2 successful markers after recovery.
  CHECK(env.disk->deadletter_calls == 3);
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
  // A05-DEVICE-T1: a committed-but-unmoved queue must be reported as Blocked,
  // not as a successful sync. The RF2 safety guarantees below are unchanged:
  // the conflict rows are still kept and the ACK still does not advance.
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Blocked);
  CHECK(c.pendingCount() == 2);  // conflict rows kept, no ACK advance
  CHECK(c.lastAcked() == 0);
  return true;
}

static bool run_case_lost_response_then_duplicate_converges() {
  Env env;
  Task t = readyTask("task-1", 3);
  {
    auto c = env.make();
    CHECK(c.applyTodaySnapshot({t}));
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
static bool run_case_boot_clock_rebase_preserves_counters_and_pending() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  env.ctx.mono = 0;  // zero is a valid monotonic epoch, not a missing anchor
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  env.ctx.advance(60000);
  auto pause = startOf();
  pause.intent = Intent::Pause;
  CHECK(c.dispatchIntent(pause, env.ctx.make()).ok);
  CHECK(c.state().active_session->actual_seconds == 60);
  CHECK(c.state().active_session->pause_count == 1);
  auto reboot = env.make();
  env.disk->fail_next_commit = true;
  CHECK(!reboot.prepareAfterBoot(0));
  CHECK(reboot.state().active_session->paused_at_monotonic_ms == 60000);
  CHECK(env.disk->state.domain.active_session->paused_at_monotonic_ms == 60000);
  CHECK(reboot.prepareAfterBoot(0));
  CHECK(reboot.pendingCount() == 3);
  CHECK(env.disk->state.next_sequence == 4);
  CHECK(reboot.state().active_session->actual_seconds == 60);
  CHECK(reboot.state().active_session->paused_at_monotonic_ms == 0);
  env.ctx.mono = 10000;
  auto resume = startOf();
  resume.intent = Intent::Resume;
  CHECK(reboot.dispatchIntent(resume, env.ctx.make()).ok);
  CHECK(reboot.state().active_session->pause_seconds == 10);
  // Reboot while running: preserve only settled time, not old-epoch uptime.
  auto running_reboot = env.make();
  CHECK(running_reboot.prepareAfterBoot(0));
  env.ctx.mono = 60000;
  auto complete = startOf();
  complete.intent = Intent::Complete;
  CHECK(running_reboot.dispatchIntent(complete, env.ctx.make()).ok);
  const auto& payload = env.disk->state.pending.back().payload;
  CHECK(payload.at("actual_seconds") == "120");
  CHECK(payload.at("pause_count") == "1");
  CHECK(running_reboot.pendingCount() == 6);
  CHECK(!running_reboot.prepareAfterBoot(-1));
  return true;
}

// ---------------------------------------------------------------------------
// WB-V53-NEXT-001 CP1 (A03): prepare / I-O / apply separation, session
// generation ownership and ACK scoping. These drive the split API directly —
// the exact path the device worker uses once the network wait is moved off the
// shared state lock. All deterministic: no threads and no sleeps.
// ---------------------------------------------------------------------------

// A server batch that accepts exactly the consecutive range [first, last].
BatchSyncResult acceptRange(int64_t first, int64_t last) {
  BatchSyncResult b;
  b.last_acked_sequence = last;
  for (int64_t s = first; s <= last; ++s) {
    PerEventResult per;
    per.event_id = EventId{"ev-" + std::to_string(s)};
    per.sequence = s;
    per.outcome = EventOutcome::Accepted;
    per.http_status = 200;
    b.results.push_back(per);
  }
  return b;
}

// RF2 helpers: build arbitrary per-event result rows.
PerEventResult mkRow(int64_t sequence, const std::string& event_id,
                     EventOutcome outcome, int http_status = 200) {
  PerEventResult per;
  per.sequence = sequence;
  per.event_id = EventId{event_id};
  per.outcome = outcome;
  per.http_status = http_status;
  return per;
}

SyncClient::Response mkResponse(int64_t last_acked,
                                std::vector<PerEventResult> rows) {
  SyncClient::Response r;
  r.error_class = SyncErrorClass::None;
  r.http_status = 200;
  r.batch.last_acked_sequence = last_acked;
  r.batch.results = std::move(rows);
  return r;
}

static bool run_case_a03_prepare_is_pure_and_frozen() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope e = c.prepareSync();
  CHECK(e.has_work);
  CHECK(!e.auth_paused);
  CHECK(e.generation == c.generation());
  CHECK(e.first_sent_sequence == 1);
  CHECK(e.max_sent_sequence == 2);
  CHECK(e.request.events.size() == 2);
  CHECK(e.request.last_acked_sequence == 0);
  // prepareSync() is pure: no I/O, no state or counter mutation.
  CHECK(c.pendingCount() == 2);
  CHECK(c.lastAcked() == 0);
  return true;
}

static bool run_case_a03_old_ack_keeps_events_queued_after_prepare() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  CHECK(sent.max_sent_sequence == 2);
  const int sent_rows = c.pendingCount();                 // == 2
  // While the worker is on the wire the state owner commits MORE events.
  env.ctx.advance(30'000);
  IntentRequest pause = startOf();
  pause.intent = Intent::Pause;
  CHECK(c.dispatchIntent(pause, env.ctx.make()).ok);
  const int queued_after_prepare = c.pendingCount();
  CHECK(queued_after_prepare > sent_rows);
  // The late response only describes the prefix that was actually sent.
  SyncClient::Response resp;
  resp.error_class = SyncErrorClass::None;
  resp.http_status = 200;
  resp.batch = acceptRange(1, 2);
  const SyncApplyOutcome out = c.applySyncResult(sent, resp);
  CHECK(out.outcome == SyncOutcome::Synced);
  CHECK(out.applied);
  CHECK(!out.stale);
  CHECK(c.lastAcked() == 2);                             // confirmed prefix only
  CHECK(c.pendingCount() == queued_after_prepare - sent_rows);
  CHECK(env.disk->state.pending.front().sequence == 3);  // post-prepare events kept
  return true;
}

static bool run_case_a03_out_of_range_ack_cannot_delete_unsent_events() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  CHECK(sent.max_sent_sequence == 2);
  env.ctx.advance(30'000);
  IntentRequest pause = startOf();
  pause.intent = Intent::Pause;
  CHECK(c.dispatchIntent(pause, env.ctx.make()).ok);      // never transmitted
  const int queued = c.pendingCount();
  CHECK(queued > 2);
  // A buggy/replayed server ACKs sequences this device never transmitted.
  SyncClient::Response resp;
  resp.error_class = SyncErrorClass::None;
  resp.http_status = 200;
  resp.batch = acceptRange(1, 4);
  resp.batch.last_acked_sequence = 4;
  const SyncApplyOutcome out = c.applySyncResult(sent, resp);
  CHECK(out.outcome == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 2);        // capped at max_sent_sequence
  CHECK(c.pendingCount() == queued - 2);  // only the genuinely sent prefix went away
  CHECK(env.disk->state.pending.front().sequence == 3);
  return true;
}

static bool run_case_a03_stale_generation_result_is_rejected() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  CHECK(sent.generation == 0);
  // Runtime reconfigured / session rebuilt while the request was in flight.
  c.beginNewSession();
  CHECK(c.generation() == 1);
  SyncClient::Response resp;
  resp.error_class = SyncErrorClass::None;
  resp.http_status = 200;
  resp.batch = acceptRange(1, 2);
  const SyncApplyOutcome out = c.applySyncResult(sent, resp);
  CHECK(out.outcome == SyncOutcome::StaleResult);
  CHECK(out.stale);
  CHECK(!out.applied);
  CHECK(c.pendingCount() == 2);   // nothing written into the new session
  CHECK(c.lastAcked() == 0);
  CHECK(env.disk->ack_calls == 0);  // ACK cleanup never even attempted
  // A fresh envelope from the new generation applies normally.
  const SyncRequestEnvelope fresh = c.prepareSync();
  CHECK(fresh.generation == 1);
  CHECK(fresh.has_work);
  const SyncApplyOutcome ok = c.applySyncResult(fresh, resp);
  CHECK(ok.outcome == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);
  CHECK(c.lastAcked() == 2);
  return true;
}

static bool run_case_a03_apply_never_performs_reauth() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  const SyncRequestEnvelope sent = c.prepareSync();
  SyncClient::Response auth_fail;
  auth_fail.error_class = SyncErrorClass::Auth;
  auth_fail.http_status = 401;
  // First auth failure only SIGNALS: no state written, no credential refresh
  // performed from inside the state transaction.
  const SyncApplyOutcome first = c.applySyncResult(sent, auth_fail);
  CHECK(first.outcome == SyncOutcome::ReauthOk);
  CHECK(!first.applied);
  CHECK(c.pendingCount() == 2);
  CHECK(!c.authPaused());
  // Second failure inside the same attempt budget pauses the transport.
  const SyncApplyOutcome second = c.applySyncResult(sent, auth_fail);
  CHECK(second.outcome == SyncOutcome::PausedAuth);
  CHECK(second.applied);
  CHECK(c.authPaused());
  CHECK(c.pendingCount() == 2);   // pending untouched by auth failures
  // While paused prepareSync() reports the gate instead of work (no send).
  const SyncRequestEnvelope paused = c.prepareSync();
  CHECK(paused.auth_paused);
  CHECK(!paused.has_work);
  // beginNewSession() clears the pause gate for the rebuilt session.
  c.beginNewSession();
  CHECK(!c.authPaused());
  CHECK(c.prepareSync().has_work);
  return true;
}

static bool run_case_a03_prepare_apply_matches_wrapper_outcome() {
  // The legacy synchronous wrapper must stay equivalent to the explicit
  // prepare -> send -> apply path, so existing Host tests, the Virtual Device
  // fault matrix and the wire fixtures keep their contract.
  Env a;
  Env b;
  CHECK(seedTask(a, readyTask("task-1", 3)));
  CHECK(seedTask(b, readyTask("task-1", 3)));
  auto ca = a.make();
  auto cb = b.make();
  CHECK(ca.dispatchIntent(startOf(), a.ctx.make()).ok);
  CHECK(cb.dispatchIntent(startOf(), b.ctx.make()).ok);

  FakeSyncTransport tra;
  const SyncOutcome wrapper = ca.runSyncOnce(tra, [] { return true; });

  const SyncRequestEnvelope sent = cb.prepareSync();
  FakeSyncTransport trb;
  const SyncClient::Response resp = trb.send(sent.request);
  const SyncApplyOutcome split = cb.applySyncResult(sent, resp);

  CHECK(wrapper == split.outcome);
  CHECK(ca.pendingCount() == cb.pendingCount());
  CHECK(ca.lastAcked() == cb.lastAcked());
  CHECK(tra.requests.size() == 1);
  CHECK(trb.requests.size() == 1);
  CHECK(tra.requests[0].events.size() == trb.requests[0].events.size());
  return true;
}

// ---------------------------------------------------------------------------
// WB-V53-NEXT-001 CP2 (A04): a stale authoritative snapshot must never
// overwrite a locally reached offline terminal state.
// ---------------------------------------------------------------------------

static bool run_case_a04_offline_complete_not_revived_by_stale_snapshot() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // Running
  env.ctx.advance(60'000);
  IntentRequest comp;
  comp.intent = Intent::Complete;
  comp.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(comp, env.ctx.make()).ok);       // Completed offline
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);
  CHECK(c.pendingCount() == 4);                           // terminal not ACKed
  // The server has not observed the completion yet and still reports Ready.
  Task stale = readyTask("task-1", 3);
  stale.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({stale}));
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);  // NOT revived
  // Even a NEWER stale revision must not revive it while un-ACKed.
  Task stale_newer = readyTask("task-1", 9);
  stale_newer.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({stale_newer}));
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);
  CHECK(c.state().tasks[0].version == 9);                     // fields refreshed
  return true;
}

static bool run_case_a04_offline_skip_not_revived_by_stale_snapshot() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  // Skip is only defined from Ready (reducer contract), so no Start here.
  IntentRequest skip;
  skip.intent = Intent::Skip;
  skip.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(skip, env.ctx.make()).ok);       // Skipped offline
  CHECK(c.state().tasks[0].status == TaskStatus::Skipped);
  CHECK(c.pendingCount() == 1);                           // terminal not ACKed
  Task stale = readyTask("task-1", 3);
  stale.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({stale}));
  CHECK(c.state().tasks[0].status == TaskStatus::Skipped);  // NOT revived
  return true;
}

static bool run_case_a04_terminal_applies_again_after_ack() {
  // Once the terminal event is ACKed the protection must lift, otherwise a
  // legitimate new plan revision could never reopen the task (no wedging).
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  env.ctx.advance(60'000);
  IntentRequest comp;
  comp.intent = Intent::Complete;
  comp.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(comp, env.ctx.make()).ok);
  Task stale = readyTask("task-1", 3);
  stale.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({stale}));
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);

  // Sync: the terminal events are now confirmed by the server.
  FakeSyncTransport tr;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 4);
  CHECK(c.pendingCount() == 0);

  // A newer authoritative revision may now reopen the task.
  Task reopened = readyTask("task-1", 9);
  reopened.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({reopened}));
  CHECK(c.state().tasks[0].status == TaskStatus::Ready);
  CHECK(c.state().tasks[0].version == 9);
  return true;
}

static bool run_case_a04_empty_snapshot_differs_from_request_failure() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  // A persistence/request failure must NOT be conflated with "no tasks today".
  env.disk->fail_next_commit = true;
  CHECK(!c.applyTodaySnapshot({}));   // snapshot commit failed
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.pendingCount() == 0);
  // A genuine empty snapshot IS authoritative and clears non-active rows.
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.empty());
  return true;
}

static bool run_case_a04_active_reschedule_and_delete_keep_session() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  const SessionId session_id = c.state().active_session->session_id;
  // Server reschedules the active task (newer version, stale Ready status).
  Task moved = readyTask("task-1", 9);
  moved.status = TaskStatus::Ready;
  moved.scheduled_date = "2026-09-20";
  CHECK(c.applyTodaySnapshot({moved}));
  CHECK(c.state().active_session.has_value());
  CHECK(c.state().active_session->session_id == session_id);  // session kept
  CHECK(c.state().tasks[0].status == TaskStatus::InProgress);  // live status kept
  CHECK(c.state().tasks[0].scheduled_date == "2026-09-20");    // reschedule applied
  CHECK(c.state().tasks[0].version == 9);
  // Server deletes the active task entirely while the session is live.
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().active_session.has_value());                 // session kept
  CHECK(c.state().tasks.size() == 1);                          // row retained
  CHECK(c.state().active_session->session_id == session_id);
  return true;
}

// ---------------------------------------------------------------------------
// REVIEW-FIX-001 RF2: the ACK may only advance a CONTIGUOUS, id-matched,
// Accepted/Duplicate prefix — never just because the server raised
// last_acked_sequence.
// ---------------------------------------------------------------------------

static bool run_case_a03_ack_stops_before_conflict_gap() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  CHECK(sent.request.events.size() == 2);
  // Server claims last_acked = 2 but reports seq2 as a Conflict.
  const SyncClient::Response resp = mkResponse(2, {
      mkRow(1, "ev-1", EventOutcome::Accepted),
      mkRow(2, "ev-2", EventOutcome::Conflict, 409),
  });
  const SyncApplyOutcome out = c.applySyncResult(sent, resp);
  CHECK(out.outcome == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 1);       // stopped before the conflict
  CHECK(c.pendingCount() == 1);    // seq2 kept
  CHECK(env.disk->state.pending.front().sequence == 2);
  return true;
}

static bool run_case_a03_ack_stops_on_missing_result() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  // seq2 has no row at all even though the server raised last_acked to 2.
  const SyncClient::Response resp = mkResponse(2, {
      mkRow(1, "ev-1", EventOutcome::Accepted),
  });
  const SyncApplyOutcome out = c.applySyncResult(sent, resp);
  CHECK(out.outcome == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 1);
  CHECK(c.pendingCount() == 1);
  return true;
}

static bool run_case_a03_ack_stops_on_wrong_event_id() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  // seq1 is Accepted but carries an event_id this device never sent.
  const SyncClient::Response resp = mkResponse(2, {
      mkRow(1, "ev-forged", EventOutcome::Accepted),
      mkRow(2, "ev-2", EventOutcome::Accepted),
  });
  const SyncApplyOutcome out = c.applySyncResult(sent, resp);
  CHECK(out.outcome == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 0);      // the forged row cannot start the prefix
  CHECK(c.pendingCount() == 2);   // nothing deleted
  CHECK(env.disk->ack_calls == 0);
  return true;
}

static bool run_case_a03_ack_handles_out_of_order_results_safely() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  // Rows delivered back-to-front; the walk must still form the full prefix.
  const SyncClient::Response resp = mkResponse(2, {
      mkRow(2, "ev-2", EventOutcome::Accepted),
      mkRow(1, "ev-1", EventOutcome::Accepted),
  });
  const SyncApplyOutcome out = c.applySyncResult(sent, resp);
  CHECK(out.outcome == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 2);
  CHECK(c.pendingCount() == 0);
  return true;
}

static bool run_case_a03_duplicate_response_is_idempotent() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  const SyncRequestEnvelope sent = c.prepareSync();
  const SyncClient::Response resp = mkResponse(2, {
      mkRow(1, "ev-1", EventOutcome::Duplicate),
      mkRow(2, "ev-2", EventOutcome::Duplicate),
  });
  CHECK(c.applySyncResult(sent, resp).outcome == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 2);
  CHECK(c.pendingCount() == 0);
  // Replaying the very same response must not double-apply anything.
  CHECK(c.applySyncResult(sent, resp).outcome == SyncOutcome::Synced);
  CHECK(c.lastAcked() == 2);
  CHECK(c.pendingCount() == 0);
  // And a fresh envelope has nothing left to send.
  CHECK(!c.prepareSync().has_work);
  return true;
}

// ---------------------------------------------------------------------------
// REVIEW-FIX-001 RF3: an un-ACKed terminal state is a TOMBSTONE that survives
// empty/stale snapshots and a reboot, and lifts only after the ACK.
// ---------------------------------------------------------------------------

static bool run_case_a04_complete_empty_snapshot_stale_ready_not_revived() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  env.ctx.advance(60'000);
  IntentRequest comp;
  comp.intent = Intent::Complete;
  comp.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(comp, env.ctx.make()).ok);
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);
  // The server temporarily does not list the task at all.
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.size() == 1);                       // tombstone retained
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);
  // Then the server's stale Ready revision shows up again: it must NOT be
  // appended as a "new" task reviving the finished one.
  Task stale = readyTask("task-1", 3);
  stale.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({stale}));
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].status == TaskStatus::Completed);
  return true;
}

static bool run_case_a04_skip_empty_snapshot_stale_ready_not_revived() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  IntentRequest skip;
  skip.intent = Intent::Skip;
  skip.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(skip, env.ctx.make()).ok);
  CHECK(c.state().tasks[0].status == TaskStatus::Skipped);
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].status == TaskStatus::Skipped);
  Task stale = readyTask("task-1", 3);
  stale.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({stale}));
  CHECK(c.state().tasks[0].status == TaskStatus::Skipped);
  return true;
}

static bool run_case_a04_terminal_protection_survives_restart() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  {
    auto c = env.make();
    CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
    env.ctx.advance(60'000);
    IntentRequest comp;
    comp.intent = Intent::Complete;
    comp.task_id = TaskId{"task-1"};
    CHECK(c.dispatchIntent(comp, env.ctx.make()).ok);
    // The server drops the row before the terminal event is ACKed.
    CHECK(c.applyTodaySnapshot({}));
    CHECK(c.state().tasks.size() == 1);
  }
  // "Reboot": a fresh coordinator over the same persisted state. The
  // protection is derived from the persisted pending queue, so it survives.
  auto c2 = env.make();
  CHECK(c2.state().tasks.size() == 1);
  CHECK(c2.state().tasks[0].status == TaskStatus::Completed);
  Task stale = readyTask("task-1", 3);
  stale.status = TaskStatus::Ready;
  CHECK(c2.applyTodaySnapshot({stale}));
  CHECK(c2.state().tasks[0].status == TaskStatus::Completed);  // not revived
  return true;
}

static bool run_case_a04_terminal_protection_lifts_after_ack() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto c = env.make();
  CHECK(c.dispatchIntent(startOf(), env.ctx.make()).ok);
  env.ctx.advance(60'000);
  IntentRequest comp;
  comp.intent = Intent::Complete;
  comp.task_id = TaskId{"task-1"};
  CHECK(c.dispatchIntent(comp, env.ctx.make()).ok);
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.size() == 1);   // protected while un-ACKed
  // The terminal events are confirmed by the server.
  FakeSyncTransport tr;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);
  // Protection lifted: the empty snapshot now removes the row, and a NEW
  // authoritative revision may legitimately reopen the task.
  CHECK(c.applyTodaySnapshot({}));
  CHECK(c.state().tasks.empty());
  Task reopened = readyTask("task-1", 9);
  reopened.status = TaskStatus::Ready;
  CHECK(c.applyTodaySnapshot({reopened}));
  CHECK(c.state().tasks.size() == 1);
  CHECK(c.state().tasks[0].status == TaskStatus::Ready);
  CHECK(c.state().tasks[0].version == 9);
  return true;
}

static bool run_case_all() {
  CASE(boot_clock_rebase_preserves_counters_and_pending);
  CASE(dispatch_start_publishes_after_commit);
  CASE(dispatch_reducer_reject_not_published);
  CASE(dispatch_outbox_failure_not_published);
  CASE(dispatch_timer_timeout_snapshot_commit);
  CASE(snapshot_version_guard_and_append);
  CASE(snapshot_a_b_to_a_removes_b);
  CASE(snapshot_keeps_running_task_status);
  CASE(snapshot_active_retained_when_server_empty);
  CASE(snapshot_empty_removes_non_active_tasks);
  CASE(snapshot_cleans_completed_after_terminal_acked);
  CASE(snapshot_persists_across_reboot);
  CASE(sync_sends_consecutive_prefix_only);
  CASE(sync_cleans_pending_on_success);
  CASE(sync_duplicate_response_converges);
  CASE(sync_auth_reauth_ok_once);
  CASE(sync_auth_reauth_fail_pauses);
  CASE(sync_auth_pause_stops_sending_until_reset);
  CASE(sync_network_backoff_grows_to_cap);
  CASE(sync_business_4xx_deadletter_keeps_pending);
  CASE(sync_deadletter_storage_failure_blocks_cleanup);
  CASE(sync_conflict_keeps_pending_no_cleanup);
  CASE(lost_response_then_duplicate_converges);
  CASE(sync_diagnostic_slot_set_on_success);
  CASE(end_to_end_start_complete_sync);
  CASE(dispatch_retry_same_event_id_after_outbox_failure);
  // WB-V53-NEXT-001 CP1 (A03)
  CASE(a03_prepare_is_pure_and_frozen);
  CASE(a03_old_ack_keeps_events_queued_after_prepare);
  CASE(a03_out_of_range_ack_cannot_delete_unsent_events);
  CASE(a03_stale_generation_result_is_rejected);
  CASE(a03_apply_never_performs_reauth);
  CASE(a03_prepare_apply_matches_wrapper_outcome);
  // WB-V53-NEXT-001 CP2 (A04)
  CASE(a04_offline_complete_not_revived_by_stale_snapshot);
  CASE(a04_offline_skip_not_revived_by_stale_snapshot);
  CASE(a04_terminal_applies_again_after_ack);
  CASE(a04_empty_snapshot_differs_from_request_failure);
  CASE(a04_active_reschedule_and_delete_keep_session);
  // REVIEW-FIX-001 RF2 (contiguous scoped ACK prefix)
  CASE(a03_ack_stops_before_conflict_gap);
  CASE(a03_ack_stops_on_missing_result);
  CASE(a03_ack_stops_on_wrong_event_id);
  CASE(a03_ack_handles_out_of_order_results_safely);
  CASE(a03_duplicate_response_is_idempotent);
  // REVIEW-FIX-001 RF3 (terminal tombstone across snapshots / reboot)
  CASE(a04_complete_empty_snapshot_stale_ready_not_revived);
  CASE(a04_skip_empty_snapshot_stale_ready_not_revived);
  CASE(a04_terminal_protection_survives_restart);
  CASE(a04_terminal_protection_lifts_after_ack);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-STREAM-002 CP3 application coordinator tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
