// claw4/firmware/tests/unit/application/ack_queue_recovery_tests.cpp
//
// A05-DEVICE-T1 (ACK_PIPELINE_FAIL) regression tests.
//
// The defect (observed on the real M0 candidate against the reviewed Learning
// Backend): the device re-sent the SAME 20 events every ~15 s forever while the
// server answered 200 / duplicate, because
//   * removeAcked() is a LOW-WATER-MARK delete (everything <= up_to goes), and
//   * scopeBatchToSent() stops the consecutive-prefix walk at the first row that
//     is not Accepted/Duplicate,
// so one permanently un-ackable row (conflict: the server's sequence slot is
// owned by a different event_id) pinned every row behind it, and the cycle was
// still reported as a successful sync.
//
// These tests pin down BOTH halves of the fix:
//   * a committed-but-unmoved queue is reported Blocked, never Synced (no more
//     silent livelock), and
//   * when the server's baseline is ahead of ours the local sequence space is
//     re-based onto it LOSSLESSLY, so the next cycle actually drains the queue.
// The RF2 safety guarantees (no ACK advance / no removal on partial, forged or
// unconfirmable answers) are asserted unchanged.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "application/coordinator.h"
#include "fakes/fake_outbox_storage.h"
#include "fakes/fake_sync_transport.h"
#include "learning_domain/reducer.h"

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

#define CHECK(cond)                                                  \
  do {                                                               \
    if (!(cond)) {                                                   \
      std::printf("  assert fail: %s (line %d)\n", #cond, __LINE__); \
      return false;                                                  \
    }                                                                \
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

  AppCoordinator make() { return AppCoordinator{storage, reducer, opts}; }
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

IntentRequest startOf() {
  IntentRequest r;
  r.intent = Intent::StartTask;
  r.task_id = TaskId{"task-1"};
  return r;
}

IntentRequest completeOf() {
  IntentRequest r;
  r.intent = Intent::Complete;
  r.task_id = TaskId{"task-1"};
  return r;
}

// Seeds one ready task, then a coordinator built AFTER that commit (the
// coordinator caches the domain state, so constructing it earlier would see an
// empty snapshot), then drives Start + Complete so that exactly four pending
// events (seq 1..4) exist with real ids, types and payloads from the reducer.
std::optional<AppCoordinator> seedFourPending(Env& env) {
  {
    auto seeder = env.make();
    if (!seeder.applyTodaySnapshot({readyTask("task-1", 3)})) return std::nullopt;
  }
  AppCoordinator c = env.make();
  if (!c.dispatchIntent(startOf(), env.ctx.make()).ok) return std::nullopt;
  env.ctx.advance(60'000);
  if (!c.dispatchIntent(completeOf(), env.ctx.make()).ok) return std::nullopt;
  if (c.pendingCount() != 4) return std::nullopt;
  return c;
}

PerEventResult mkRow(int64_t sequence, const EventId& event_id,
                     EventOutcome outcome, int http_status = 200) {
  PerEventResult per;
  per.sequence = sequence;
  per.event_id = event_id;
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

// The exact shape the real backend returned for the real M0 device: the first
// rows conflict (their slots are owned by other event ids), the later rows are
// already stored (duplicate) and the server's baseline (7) is ahead of ours (0).
SyncClient::Response desyncResponse(const SyncRequestEnvelope& sent) {
  std::vector<PerEventResult> rows;
  for (const auto& ev : sent.request.events) {
    const bool conflict = ev.sequence <= 2;
    rows.push_back(mkRow(ev.sequence, ev.event_id,
                         conflict ? EventOutcome::Conflict : EventOutcome::Duplicate,
                         conflict ? 409 : 200));
  }
  return mkResponse(7, std::move(rows));
}

std::set<std::string> idsOf(const std::vector<PendingEvent>& rows) {
  std::set<std::string> ids;
  for (const auto& p : rows) ids.insert(p.event_id.value);
  return ids;
}

std::vector<int64_t> sequencesOf(const std::vector<PendingEvent>& rows) {
  std::vector<int64_t> seqs;
  for (const auto& p : rows) seqs.push_back(p.sequence);
  return seqs;
}

// ---------------------------------------------------------------------------
// cases
// ---------------------------------------------------------------------------

// 1) The defect itself: a complete answer that confirms nothing must not be
//    reported as a successful sync, and must not remove anything.
static bool run_case_blocked_queue_is_not_reported_as_synced() {
  Env env;
  auto co = seedFourPending(env);
  CHECK(co.has_value());
  AppCoordinator& c = *co;
  CHECK(c.lastAcked() == 0);

  const SyncRequestEnvelope sent = c.prepareSync();
  CHECK(sent.request.events.size() == 4);
  std::vector<PerEventResult> rows;
  for (const auto& ev : sent.request.events) {
    rows.push_back(mkRow(ev.sequence, ev.event_id, EventOutcome::Conflict, 409));
  }
  const SyncApplyOutcome out = c.applySyncResult(
      sent, mkResponse(sent.request.last_acked_sequence, std::move(rows)));

  CHECK(out.outcome == SyncOutcome::Blocked);     // NOT Synced
  CHECK(!out.rebased);
  CHECK(env.disk->ack_calls == 0);                // removeAcked() never called
  CHECK(c.pendingCount() == 4);                   // nothing removed
  CHECK(c.lastAcked() == 0);                      // ACK did not advance
  CHECK(env.disk->state.pending.size() == 4);
  CHECK(env.disk->state.diagnostic.sync_failed);  // the blocker is visible
  return true;
}

// 2) Lossless recovery: server baseline ahead of ours -> re-base (no event lost,
//    ids/payloads/metadata preserved) and the very next cycle drains the queue.
static bool run_case_desync_is_rebased_losslessly_then_drains() {
  Env env;
  auto co = seedFourPending(env);
  CHECK(co.has_value());
  AppCoordinator& c = *co;

  const std::vector<PendingEvent> before = env.disk->state.pending;
  const std::set<std::string> ids_before = idsOf(before);
  const SyncRequestEnvelope sent = c.prepareSync();
  CHECK(sent.request.events.size() == 4);

  const SyncApplyOutcome out = c.applySyncResult(sent, desyncResponse(sent));
  CHECK(out.outcome == SyncOutcome::Blocked);  // honest: this cycle delivered nothing
  CHECK(out.rebased);                          // but the desync was repaired
  CHECK(env.disk->rebase_calls == 1);

  // Baseline adopted, rows re-numbered above it, nothing dropped, order kept.
  CHECK(c.lastAcked() == 7);
  CHECK(c.pendingCount() == 4);
  CHECK((sequencesOf(env.disk->state.pending) == std::vector<int64_t>{8, 9, 10, 11}));
  CHECK(idsOf(env.disk->state.pending) == ids_before);  // lossless
  for (std::size_t i = 0; i < before.size(); ++i) {
    const PendingEvent& now = env.disk->state.pending[i];
    CHECK(now.event_id.value == before[i].event_id.value);
    CHECK(now.type == before[i].type);
    CHECK(now.version == before[i].version);
    CHECK(now.timestamp == before[i].timestamp);
    CHECK(now.timestamp_source == before[i].timestamp_source);
    CHECK(now.payload == before[i].payload);
  }
  CHECK(env.disk->state.next_sequence == 12);  // no later allocation collides

  // Next cycle: everything sits in fresh, free slots -> accepted -> drained.
  FakeSyncTransport tr2;
  const SyncOutcome oc = c.runSyncOnce(tr2, [] { return true; });
  CHECK(oc == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);
  CHECK(c.lastAcked() == 11);
  CHECK(tr2.requests.size() == 1);
  CHECK(tr2.requests[0].last_acked_sequence == 7);
  return true;
}

// 3) The re-base is an explicit option: with it off the old (livelocking)
//    behaviour is preserved bit for bit, for A/B comparison and review.
static bool run_case_rebase_disabled_keeps_old_behaviour() {
  Env env;
  env.opts.allow_baseline_rebase = false;
  auto co = seedFourPending(env);
  CHECK(co.has_value());
  AppCoordinator& c = *co;

  const SyncRequestEnvelope sent = c.prepareSync();
  const SyncApplyOutcome out = c.applySyncResult(sent, desyncResponse(sent));
  CHECK(out.outcome == SyncOutcome::Blocked);  // no longer claims success
  CHECK(!out.rebased);
  CHECK(env.disk->rebase_calls == 0);
  CHECK(c.lastAcked() == 0);                   // unchanged
  CHECK(c.pendingCount() == 4);
  CHECK((sequencesOf(env.disk->state.pending) == std::vector<int64_t>{1, 2, 3, 4}));
  return true;
}

// 4) Fail-closed: if the storage cannot re-base, nothing may change and the
//    queue must be reported blocked (never silently dropped, never "Synced").
static bool run_case_rebase_failure_is_fail_closed() {
  Env env;
  auto co = seedFourPending(env);
  CHECK(co.has_value());
  AppCoordinator& c = *co;

  const SyncRequestEnvelope sent = c.prepareSync();
  env.disk->fail_next_commit = true;  // injection: the re-base write fails

  const SyncApplyOutcome out = c.applySyncResult(sent, desyncResponse(sent));
  CHECK(out.outcome == SyncOutcome::Blocked);
  CHECK(!out.rebased);
  CHECK(c.lastAcked() == 0);  // atomic: nothing visible changed
  CHECK(c.pendingCount() == 4);
  CHECK((sequencesOf(env.disk->state.pending) == std::vector<int64_t>{1, 2, 3, 4}));
  return true;
}

// 5) RF2 is preserved: a partial or forged answer must never trigger a re-base
//    (only a COMPLETE, id-matched answer may be read as "queue blocked").
static bool run_case_partial_answer_never_triggers_rebase() {
  {
    Env env;
    auto co = seedFourPending(env);
    CHECK(co.has_value());
    AppCoordinator& c = *co;

    const SyncRequestEnvelope sent = c.prepareSync();
    CHECK(sent.request.events.size() == 4);
    // Only the first two rows come back, yet the server claims a far-ahead ACK.
    std::vector<PerEventResult> rows;
    rows.push_back(mkRow(1, sent.request.events[0].event_id, EventOutcome::Conflict, 409));
    rows.push_back(mkRow(2, sent.request.events[1].event_id, EventOutcome::Conflict, 409));

    const SyncApplyOutcome out = c.applySyncResult(sent, mkResponse(7, std::move(rows)));
    // The RF2 / legacy behaviour for partial answers is deliberately untouched:
    // an incomplete answer is never read as "queue blocked", so the outcome stays
    // Synced while the ACK and the rows are protected by the prefix walk.
    CHECK(out.outcome == SyncOutcome::Synced);
    CHECK(!out.rebased);
    CHECK(env.disk->rebase_calls == 0);
    CHECK(c.lastAcked() == 0);
    CHECK(c.pendingCount() == 4);
    CHECK((sequencesOf(env.disk->state.pending) == std::vector<int64_t>{1, 2, 3, 4}));
  }
  {
    // Same, with a forged event id on the first row.
    Env env;
    auto co = seedFourPending(env);
    CHECK(co.has_value());
    AppCoordinator& c = *co;

    const SyncRequestEnvelope sent = c.prepareSync();
    std::vector<PerEventResult> forged;
    for (const auto& ev : sent.request.events) {
      forged.push_back(mkRow(ev.sequence, ev.event_id, EventOutcome::Conflict, 409));
    }
    forged[0].event_id = EventId{"ev-forged"};

    const SyncApplyOutcome out = c.applySyncResult(sent, mkResponse(7, std::move(forged)));
    CHECK(!out.rebased);
    CHECK(env.disk->rebase_calls == 0);
    CHECK(c.lastAcked() == 0);
    CHECK(c.pendingCount() == 4);
  }
  return true;
}

// 6) A normal successful batch still reports Synced and drains (no regression).
static bool run_case_success_still_works() {
  Env env;
  auto co = seedFourPending(env);
  CHECK(co.has_value());
  AppCoordinator& c = *co;

  FakeSyncTransport tr;
  CHECK(c.runSyncOnce(tr, [] { return true; }) == SyncOutcome::Synced);
  CHECK(c.pendingCount() == 0);
  CHECK(c.lastAcked() == 4);
  CHECK(env.disk->rebase_calls == 0);
  return true;
}

static bool run_case_all() {
  CASE(blocked_queue_is_not_reported_as_synced);
  CASE(desync_is_rebased_losslessly_then_drains);
  CASE(rebase_disabled_keeps_old_behaviour);
  CASE(rebase_failure_is_fail_closed);
  CASE(partial_answer_never_triggers_rebase);
  CASE(success_still_works);
  return g_fail == 0;
}

int main() {
  std::printf("== A05-DEVICE-T1 ack queue recovery tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
