// claw4/firmware/tests/unit/reminder/reminder_core_tests.cpp
// WB-V53-NEXT-001 CP4 (B03) — host tests for the reminder core.
// Deterministic: injected clock, injected store, no threads, no sleeps, no
// esp_sleep/LVGL and no real audio.

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "ports/clock_port.h"
#include "reminder/reminder_core.h"
#include "time/time_authority.h"

using claw4::domain::ChildId;
using claw4::domain::TaskId;
using claw4::ports::ClockPort;
using claw4::reminder::ReminderCore;
using claw4::reminder::ReminderKind;
using claw4::reminder::ReminderPolicy;
using claw4::reminder::ReminderRecord;
using claw4::reminder::ReminderStore;
using claw4::time::TimeAuthority;
using claw4::time::TimeAuthorityConfig;

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
class FakeClock final : public ClockPort {
 public:
  int64_t mono = 0;
  int64_t epochSeconds() override { return 0; }
  int64_t monotonicMs() override { return mono; }
  bool isTimeSynced() override { return true; }
};

class FakeReminderStore final : public ReminderStore {
 public:
  std::vector<ReminderRecord> records;
  bool fail_saves = false;
  int save_calls = 0;

  bool load(std::vector<ReminderRecord>& out) override {
    out = records;
    return true;
  }
  bool save(const std::vector<ReminderRecord>& in) override {
    ++save_calls;
    if (fail_saves) return false;
    records = in;
    return true;
  }
};

// 12:00 local (outside the default quiet period).
static constexpr int64_t kNoon = 12 * 3600 * 1000;
static constexpr int64_t kMidnight30 = 30 * 60 * 1000;   // 00:30 local
static constexpr int64_t kBaseEpoch = 1'756'718'530'000;

struct Harness {
  FakeClock clock;
  TimeAuthority authority{clock, "boot-a", TimeAuthorityConfig{60000, 600000, 60000, 100}};
  FakeReminderStore store;
  ReminderPolicy policy;
  // unique_ptr so a test can change `policy` and then rebuild the core: the
  // core owns a COPY of the policy (it is a value configuration).
  std::unique_ptr<ReminderCore> core;

  Harness() { build(); }
  void build() { core = std::make_unique<ReminderCore>(policy, store, authority); }
  // Brings the authority into the "calendar allowed" state.
  bool trustTime() {
    const auto r = authority.acceptSync(kBaseEpoch, "net", 1000);
    return r.outcome == claw4::time::SyncOutcome::Accepted;
  }
};

ReminderRecord makeRecord(ReminderKind kind, const std::string& task,
                          int64_t due_epoch_ms) {
  ReminderRecord r;
  r.kind = kind;
  r.child_id = ChildId{"child-1"};
  r.task_id = TaskId{task};
  r.due_epoch_ms = due_epoch_ms;
  r.instance_id = ReminderCore::makeInstanceId(kind, r.child_id, r.task_id,
                                               due_epoch_ms);
  return r;
}

// ---------------------------------------------------------------------------
static bool run_case_stable_instance_id_dedupes_on_rebuild() {
  Harness h;
  CHECK(h.trustTime());
  const ReminderRecord due = makeRecord(ReminderKind::TaskDue, "task-1",
                                        kBaseEpoch + 60'000);
  CHECK(h.core->upsert(due));
  CHECK(h.core->activeCount() == 1);
  // The same logical reminder must map to the same id (no duplicate on re-add).
  CHECK(h.core->upsert(due));
  CHECK(h.core->activeCount() == 1);
  CHECK(h.core->records().size() == 1);
  // A rebuild from persistence keeps exactly one instance.
  CHECK(h.core->load());
  CHECK(h.core->activeCount() == 1);
  CHECK(h.core->records()[0].instance_id == due.instance_id);
  return true;
}

static bool run_case_rebuild_preserves_snooze() {
  Harness h;
  CHECK(h.trustTime());
  const ReminderRecord due = makeRecord(ReminderKind::TaskDue, "task-1",
                                        kBaseEpoch + 60'000);
  CHECK(h.core->upsert(due));
  CHECK(h.core->snooze(due.instance_id, kBaseEpoch));
  const int64_t snoozed_to = h.core->records()[0].snooze_until_epoch_ms;
  CHECK(snoozed_to == kBaseEpoch + h.policy.snooze_ms);
  // Rebuild (reboot / runtime reconfigure) must NOT reset the snooze.
  CHECK(h.core->load());
  CHECK(h.core->records().size() == 1);
  CHECK(h.core->records()[0].snooze_until_epoch_ms == snoozed_to);
  CHECK(h.core->records()[0].delivered_epoch_ms == 0);
  return true;
}

static bool run_case_cancel_on_complete_reschedule_delete() {
  Harness h;
  CHECK(h.trustTime());
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch + 1000)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::Snooze, "task-1", kBaseEpoch + 2000)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::FocusEnd, "task-2", kBaseEpoch + 3000)));
  CHECK(h.core->activeCount() == 3);
  // Complete / reschedule / delete of task-1 cancels all of its reminders.
  CHECK(h.core->cancelForTask(TaskId{"task-1"}));
  CHECK(h.core->activeCount() == 1);
  CHECK(h.core->records().size() == 1);
  CHECK(h.core->records()[0].task_id == TaskId{"task-2"});
  // Cancelling an unknown task is a harmless no-op.
  CHECK(h.core->cancelForTask(TaskId{"task-9"}));
  CHECK(h.core->activeCount() == 1);
  // Replacing the whole plan for the child clears the rest.
  CHECK(h.core->cancelForChild(ChildId{"child-1"}));
  CHECK(h.core->activeCount() == 0);
  return true;
}

static bool run_case_all_four_kinds_supported() {
  Harness h;
  CHECK(h.trustTime());
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "t1", kBaseEpoch + 1000)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::Snooze, "t2", kBaseEpoch + 2000)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::FocusEnd, "t3", kBaseEpoch + 3000)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::DailyReview, "t4", kBaseEpoch + 4000)));
  CHECK(h.core->activeCount() == 4);
  return true;
}

static bool run_case_capacity_bounded_and_separate_from_outbox() {
  Harness h;
  h.policy.max_instances = 3;
  h.build();  // the core owns a copy of the policy
  CHECK(h.trustTime());
  for (int i = 0; i < 3; ++i) {
    CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue,
                                   "task-" + std::to_string(i),
                                   kBaseEpoch + 1000 * (i + 1))));
  }
  CHECK(h.core->activeCount() == 3);
  // 4th instance is rejected: the reminder table has its OWN bounded budget
  // and can never grow into the learning outbox capacity.
  CHECK(!h.core->upsert(makeRecord(ReminderKind::TaskDue, "task-4",
                                  kBaseEpoch + 9000)));
  CHECK(h.core->diagnostics().rejected_capacity == 1);
  CHECK(h.core->activeCount() == 3);
  return true;
}

static bool run_case_untrusted_time_blocks_calendar_reminders() {
  Harness h;
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch)));
  // No acceptSync() yet: the authority reports Unsynced.
  CHECK(h.core->collectDue(kNoon).empty());
  CHECK(h.core->diagnostics().suppressed_untrusted_time == 1);
  CHECK(!h.core->diagnostics().last_evaluation_time_trusted);
  // Sync but let the age exceed the holdover window (untrusted again).
  CHECK(h.trustTime());
  h.clock.mono = 700'000;  // > max_holdover_ms (600000)
  CHECK(h.core->collectDue(kNoon).empty());
  CHECK(!h.core->diagnostics().last_evaluation_time_trusted);
  return true;
}

static bool run_case_forward_jump_does_not_replay_history() {
  Harness h;
  CHECK(h.trustTime());
  // A reminder that was due long ago, never presented.
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "task-1",
                                 kBaseEpoch - 10 * 60 * 1000)));
  // Clock is now well past the grace window.
  const auto due = h.core->collectDue(kNoon);
  CHECK(due.empty());                                    // NOT replayed
  CHECK(h.core->diagnostics().expired_total == 1);
  CHECK(h.core->activeCount() == 0);                      // retired, not pending
  return true;
}

static bool run_case_quiet_period_suppresses_but_keeps_pending() {
  Harness h;
  CHECK(h.trustTime());
  const ReminderRecord r = makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch);
  CHECK(h.core->upsert(r));
  // 00:30 local is inside the default quiet window (21:00-07:00).
  CHECK(h.core->collectDue(kMidnight30).empty());
  CHECK(h.core->diagnostics().suppressed_quiet == 1);
  CHECK(h.core->activeCount() == 1);                      // still pending
  CHECK(h.core->records()[0].delivered_epoch_ms == 0);
  // Once the quiet period ends the reminder is delivered.
  const auto due = h.core->collectDue(kNoon);
  CHECK(due.size() == 1);
  CHECK(due[0].instance_id == r.instance_id);
  return true;
}

static bool run_case_delivery_is_capped_and_marked_once() {
  Harness h;
  CHECK(h.trustTime());
  for (int i = 0; i < 4; ++i) {
    CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue,
                                    "task-" + std::to_string(i),
                                    kBaseEpoch - 1000 + i)));
  }
  // Default policy caps deliveries at 2 per tick.
  const auto first = h.core->collectDue(kNoon);
  CHECK(first.size() == 2);                              // capped per tick
  CHECK(first[0].due_epoch_ms <= first[1].due_epoch_ms);  // due order
  for (const auto& r : first) CHECK(h.core->markDelivered(r.instance_id, kBaseEpoch));
  // Delivered instances never fire again.
  const auto second = h.core->collectDue(kNoon);
  CHECK(second.size() == 2);
  for (const auto& r : second) {
    CHECK(r.instance_id != first[0].instance_id);
    CHECK(r.instance_id != first[1].instance_id);
  }
  for (const auto& r : second) CHECK(h.core->markDelivered(r.instance_id, kBaseEpoch));
  CHECK(h.core->collectDue(kNoon).empty());
  CHECK(h.core->diagnostics().delivered_total == 4);
  return true;
}

static bool run_case_save_failure_rolls_back() {
  Harness h;
  CHECK(h.trustTime());
  const ReminderRecord r = makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch);
  CHECK(h.core->upsert(r));
  const int saves_before = h.store.save_calls;
  h.store.fail_saves = true;
  // Snooze must NOT be half-applied when persistence fails.
  CHECK(!h.core->snooze(r.instance_id, kBaseEpoch));
  CHECK(h.core->records()[0].snooze_until_epoch_ms == 0);
  CHECK(h.core->diagnostics().save_failures == 1);
  // Marking delivered must not be reported as done either.
  CHECK(!h.core->markDelivered(r.instance_id, kBaseEpoch));
  CHECK(h.core->records()[0].delivered_epoch_ms == 0);
  CHECK(h.store.save_calls == saves_before + 2);
  // After the storage recovers the same operations succeed.
  h.store.fail_saves = false;
  CHECK(h.core->snooze(r.instance_id, kBaseEpoch));
  CHECK(h.core->records()[0].snooze_until_epoch_ms == kBaseEpoch + h.policy.snooze_ms);
  return true;
}

static bool run_case_all() {
  CASE(stable_instance_id_dedupes_on_rebuild);
  CASE(rebuild_preserves_snooze);
  CASE(cancel_on_complete_reschedule_delete);
  CASE(all_four_kinds_supported);
  CASE(capacity_bounded_and_separate_from_outbox);
  CASE(untrusted_time_blocks_calendar_reminders);
  CASE(forward_jump_does_not_replay_history);
  CASE(quiet_period_suppresses_but_keeps_pending);
  CASE(delivery_is_capped_and_marked_once);
  CASE(save_failure_rolls_back);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 CP4 reminder core (B03) tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
