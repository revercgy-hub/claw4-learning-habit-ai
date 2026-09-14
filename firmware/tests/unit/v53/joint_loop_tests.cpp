// claw4/firmware/tests/unit/v53/joint_loop_tests.cpp
// WB-V53-NEXT-001 CP5 — joint host gate: startup -> offline learning -> stale
// snapshot -> sync/ACK -> reminder state restore -> audio arbitration.
//
// Every step drives REAL production C++; only the platform boundaries are
// mocked. Mock-to-production map:
//
//   FakeOutboxStorage        -> sync::OutboxStorage (NVS adapter is device-only)
//   ProgrammableTransport    -> application::SyncTransport (real HTTP is device)
//   FakeClock                -> ports::ClockPort (esp_timer adapter is device)
//   FakeReminderStore        -> reminder::ReminderStore (NVS adapter is device)
//
// Production code exercised (not re-implemented here):
//   learning_domain::DomainReducer, application::AppCoordinator (prepare/apply/
//   applyTodaySnapshot), sync::SyncExecutor, sync::OutboxCore,
//   time::TimeAuthority, interaction::InteractionArbiter, reminder::ReminderCore.
//
// Deterministic: no threads, no sleeps, no loopback sockets, no real family
// backend, no device, no NVS. Synthetic data only.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "application/coordinator.h"
#include "interaction/interaction_arbiter.h"
#include "learning_domain/reducer.h"
#include "ports/clock_port.h"
#include "reminder/reminder_core.h"
#include "sync/sync_executor.h"
#include "fakes/fake_outbox_storage.h"

using namespace claw4::application;
using namespace claw4::domain;
using namespace claw4::sync;

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

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      std::printf("  assert fail: %s (line %d)\n", #cond, __LINE__);  \
      return false;                                                   \
    }                                                                 \
  } while (0)

// ---------------------------------------------------------------------------
// mocks (platform boundaries only)
// ---------------------------------------------------------------------------
class FakeClockPort final : public claw4::ports::ClockPort {
 public:
  int64_t mono = 0;
  int64_t epochSeconds() override { return epoch_ms / 1000; }
  int64_t monotonicMs() override { return mono; }
  bool isTimeSynced() override { return true; }
  int64_t epoch_ms = 1'756'718'530'000LL;
};

class ScenarioTransport final : public SyncTransport {
 public:
  std::function<SyncClient::Response(const SyncClient::Request&)> on_send;
  int sends = 0;

  SyncClient::Response send(const SyncClient::Request& request) override {
    ++sends;
    return on_send(request);
  }
};

SyncClient::Response acceptAll(const SyncClient::Request& request) {
  SyncClient::Response r;
  r.error_class = SyncErrorClass::None;
  r.http_status = 200;
  int64_t ack = request.last_acked_sequence;
  for (const auto& e : request.events) {
    PerEventResult per;
    per.event_id = e.event_id;
    per.sequence = e.sequence;
    per.outcome = EventOutcome::Accepted;
    per.http_status = 200;
    r.batch.results.push_back(per);
    ack = e.sequence;
  }
  r.batch.last_acked_sequence = ack;
  return r;
}

class ScenarioReminderStore final : public claw4::reminder::ReminderStore {
 public:
  std::vector<claw4::reminder::ReminderRecord> records;
  bool load(std::vector<claw4::reminder::ReminderRecord>& out) override {
    out = records;
    return true;
  }
  bool save(const std::vector<claw4::reminder::ReminderRecord>& in) override {
    records = in;
    return true;
  }
};

// ---------------------------------------------------------------------------
// one scenario = one synthetic family loop
// ---------------------------------------------------------------------------
struct Scenario {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  CoordinatorOptions opts;
  int ev = 0, sess = 0;
  int64_t mono = 1'000'000;
  int64_t epoch_ms = 1'756'718'530'000LL;

  AppCoordinator make() { return AppCoordinator{storage, reducer, opts}; }

  ReducerContext ctx() {
    ReducerContext c;
    c.device_id = DeviceId{"dev-1"};
    c.child_id = ChildId{"child-1"};
    c.now_epoch = epoch_ms / 1000;
    c.monotonic_ms = mono;
    c.make_event_id = [this] { return EventId{"ev-" + std::to_string(++ev)}; };
    c.make_session_id = [this] { return SessionId{"sess-" + std::to_string(++sess)}; };
    return c;
  }
};

Task todayTask(const std::string& id, int version) {
  Task t;
  t.task_id = TaskId{id};
  t.child_id = ChildId{"child-1"};
  t.title = "task-" + id;
  t.status = TaskStatus::Ready;
  t.version = version;
  return t;
}

IntentRequest startOf(const std::string& tid) {
  IntentRequest r;
  r.intent = Intent::StartTask;
  r.task_id = TaskId{tid};
  return r;
}

// ---------------------------------------------------------------------------
// the joint loop
// ---------------------------------------------------------------------------
static bool run_case_joint_startup_offline_stale_snapshot_sync_reminder() {
  Scenario s;
  // --- 1) startup: an empty/absent persisted state seeds the plan ----------
  auto coord = s.make();
  CHECK(coord.state().tasks.empty());
  CHECK(coord.applyTodaySnapshot({todayTask("task-1", 3)}));
  CHECK(coord.state().tasks.size() == 1);

  // --- 2) offline learning: Start then Complete with no network ------------
  CHECK(coord.dispatchIntent(startOf("task-1"), s.ctx()).ok);
  CHECK(coord.state().tasks[0].status == TaskStatus::InProgress);
  s.mono += 60'000;
  IntentRequest comp;
  comp.intent = Intent::Complete;
  comp.task_id = TaskId{"task-1"};
  CHECK(coord.dispatchIntent(comp, s.ctx()).ok);
  CHECK(coord.state().tasks[0].status == TaskStatus::Completed);
  const int pending_after_complete = coord.pendingCount();
  CHECK(pending_after_complete > 0);          // nothing ACKed yet
  CHECK(coord.lastAcked() == 0);

  // --- 3) a STALE authoritative snapshot arrives first ---------------------
  Task stale = todayTask("task-1", 3);
  stale.status = TaskStatus::Ready;           // server has not seen the event
  CHECK(coord.applyTodaySnapshot({stale}));
  CHECK(coord.state().tasks[0].status == TaskStatus::Completed);  // not revived

  // --- 4) sync: prepare (locked) -> I/O (unlocked) -> apply (locked) -------
  ScenarioTransport transport;
  transport.on_send = acceptAll;
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); });
  const SyncCycleResult cycle = executor.runCycle([] { return false; });
  CHECK(cycle.outcome == SyncOutcome::Synced);
  CHECK(cycle.transport_calls == 1);
  CHECK(coord.pendingCount() == 0);
  CHECK(coord.lastAcked() == pending_after_complete);
  // The stale row is now purely historical: a NEW revision still wins.
  Task reopened = todayTask("task-1", 9);
  reopened.status = TaskStatus::Ready;
  CHECK(coord.applyTodaySnapshot({reopened}));
  CHECK(coord.state().tasks[0].status == TaskStatus::Ready);
  CHECK(coord.state().tasks[0].version == 9);

  // --- 5) reminder state survives a rebuild --------------------------------
  FakeClockPort clock;
  clock.mono = s.mono;
  clock.epoch_ms = s.epoch_ms;
  claw4::time::TimeAuthority authority(
      clock, "boot-1", claw4::time::TimeAuthorityConfig{60000, 3600000, 60000, 100});
  CHECK(authority.acceptSync(s.epoch_ms, "net", 1000).outcome ==
        claw4::time::SyncOutcome::Accepted);
  ScenarioReminderStore reminder_store;
  claw4::reminder::ReminderPolicy policy;
  claw4::reminder::ReminderCore reminders(policy, reminder_store, authority);

  claw4::reminder::ReminderRecord due;
  due.kind = claw4::reminder::ReminderKind::TaskDue;
  due.child_id = ChildId{"child-1"};
  due.task_id = TaskId{"task-1"};
  due.due_epoch_ms = s.epoch_ms - 1000;
  due.instance_id = claw4::reminder::ReminderCore::makeInstanceId(
      due.kind, due.child_id, due.task_id, due.due_epoch_ms);
  CHECK(reminders.upsert(due));
  CHECK(reminders.snooze(due.instance_id, s.epoch_ms));
  const int64_t snoozed_to = reminders.records()[0].snooze_until_epoch_ms;
  CHECK(reminders.load());                                    // "rebuild"
  CHECK(reminders.records()[0].snooze_until_epoch_ms == snoozed_to);

  // Delivery is blocked by untrusted time, then allowed once synced.
  const auto due_now = reminders.collectDue(12 * 3600 * 1000);
  CHECK(due_now.empty());                                     // snoozed: not yet
  clock.mono += (snoozed_to - s.epoch_ms) + 1000;             // past the snooze
  const auto after_snooze = reminders.collectDue(12 * 3600 * 1000);
  CHECK(after_snooze.size() == 1);
  CHECK(after_snooze[0].instance_id == due.instance_id);

  // --- 6) audio arbitration + authorization --------------------------------
  claw4::interaction::InteractionArbiter arbiter(1);
  claw4::interaction::InteractionRequest tts;
  tts.source = claw4::interaction::AudioSource::CloudTts;
  tts.generation = 1;
  CHECK(arbiter.requestPlayback(tts, 0) == claw4::interaction::ArbiterVerdict::Granted);
  claw4::interaction::InteractionRequest ring;
  ring.source = claw4::interaction::AudioSource::LocalReminder;
  ring.child_id = ChildId{"child-1"};
  ring.task_id = TaskId{"task-1"};
  ring.reminder_instance_id = due.instance_id;
  ring.generation = 1;
  ring.explicit_user_intent = true;
  CHECK(arbiter.requestPlayback(ring, 10) ==
        claw4::interaction::ArbiterVerdict::Deferred);
  CHECK(arbiter.expireDeferredReminder(10 + 3000 + 1));
  CHECK(arbiter.currentSource() == claw4::interaction::AudioSource::LocalReminder);

  claw4::interaction::ReminderScope scope;
  scope.child_id = ChildId{"child-1"};
  scope.task_id = TaskId{"task-1"};
  scope.reminder_instance_id = due.instance_id;
  scope.generation = 1;
  CHECK(arbiter.authorizeReminderAction(ring, scope,
                                        claw4::interaction::ReminderAction::Acknowledge) ==
        claw4::interaction::ArbiterVerdict::Granted);
  // ACK is NOT a completion: the physical confirmation is still required.
  CHECK(arbiter.authorizeTaskCompletion(ring, scope, false) ==
        claw4::interaction::ArbiterVerdict::Denied);
  CHECK(arbiter.authorizeTaskCompletion(ring, scope, true) ==
        claw4::interaction::ArbiterVerdict::Granted);
  // A proactive AI can never authorize on the child's behalf.
  claw4::interaction::InteractionRequest ai;
  ai.source = claw4::interaction::AudioSource::ProactiveAi;
  ai.explicit_user_intent = true;  // even forged
  ai.child_id = scope.child_id;
  ai.task_id = scope.task_id;
  ai.reminder_instance_id = scope.reminder_instance_id;
  ai.generation = scope.generation;
  CHECK(arbiter.authorizeReminderAction(ai, scope,
                                        claw4::interaction::ReminderAction::Snooze) ==
        claw4::interaction::ArbiterVerdict::Denied);
  CHECK(arbiter.authorizeTaskCompletion(ai, scope, true) ==
        claw4::interaction::ArbiterVerdict::Denied);
  return true;
}

// A restart in the middle of the loop must converge, not duplicate work.
static bool run_case_joint_restart_convergence_is_idempotent() {
  Scenario s;
  {
    auto coord = s.make();
    CHECK(coord.applyTodaySnapshot({todayTask("task-1", 3)}));
    CHECK(coord.dispatchIntent(startOf("task-1"), s.ctx()).ok);
    s.mono += 60'000;
    IntentRequest comp;
    comp.intent = Intent::Complete;
    comp.task_id = TaskId{"task-1"};
    CHECK(coord.dispatchIntent(comp, s.ctx()).ok);
  }
  // "Reboot": a fresh coordinator over the same persisted state.
  auto coord2 = s.make();
  CHECK(coord2.pendingCount() > 0);
  CHECK(coord2.state().tasks[0].status == TaskStatus::Completed);
  const int pending_before = coord2.pendingCount();

  // A duplicate response (server already persisted the events) converges.
  ScenarioTransport transport;
  transport.on_send = [](const SyncClient::Request& request) {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    int64_t ack = request.last_acked_sequence;
    for (const auto& e : request.events) {
      PerEventResult per;
      per.event_id = e.event_id;
      per.sequence = e.sequence;
      per.outcome = EventOutcome::Duplicate;
      per.http_status = 200;
      r.batch.results.push_back(per);
      ack = e.sequence;
    }
    r.batch.last_acked_sequence = ack;
    return r;
  };
  std::mutex state_mutex;
  SyncExecutor executor(coord2, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); });
  CHECK(executor.runCycle([] { return false; }).outcome == SyncOutcome::Synced);
  CHECK(coord2.pendingCount() == 0);
  CHECK(coord2.lastAcked() == pending_before);

  // A second sync has nothing to send and never duplicates events.
  const SyncCycleResult again = executor.runCycle([] { return false; });
  CHECK(again.outcome == SyncOutcome::NoPending);
  CHECK(!again.transport_called);
  CHECK(transport.sends == 1);
  return true;
}

static bool run_case_all() {
  CASE(joint_startup_offline_stale_snapshot_sync_reminder);
  CASE(joint_restart_convergence_is_idempotent);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 CP5 joint host loop tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
