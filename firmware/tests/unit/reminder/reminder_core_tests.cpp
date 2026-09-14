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
#include "fakes/fake_reminder_wake_port.h"

using claw4::domain::ChildId;
using claw4::domain::TaskId;
using claw4::fakes::FakeReminderWakePort;
using claw4::ports::ClockPort;
using claw4::reminder::ReminderBatch;
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

// ---------------------------------------------------------------------------
// REVIEW-FIX-001 RF5: quiet vs grace, snooze-preserving refresh, capacity,
// merging, power-loss recovery and the wake port.
// ---------------------------------------------------------------------------

// Aligned synthetic day: kDayBase is treated as 00:00 of the local day and the
// authority is synced exactly at kDayBase, so
//   epoch          == kDayBase + clock.mono
//   local_of_day   == (epoch - kDayBase) % 24h
// Every quiet-period assertion advances the real epoch through the authority.
static constexpr int64_t kHour = 3600 * 1000;
static constexpr int64_t kDay = 24 * kHour;
static constexpr int64_t kDayBase = 1'756'771'200'000LL;  // divisible by 24 h

struct QuietHarness {
  FakeClock clock;
  TimeAuthority authority{
      clock, "boot-q",
      TimeAuthorityConfig{40 * kHour, 48 * kHour, 60000, 100}};
  FakeReminderStore store;
  ReminderPolicy policy;
  std::unique_ptr<ReminderCore> core;

  QuietHarness() {
    authority.acceptSync(kDayBase, "net", 1000);
    core = std::make_unique<ReminderCore>(policy, store, authority);
  }
  void setEpoch(int64_t epoch) { clock.mono = epoch - kDayBase; }
  static int64_t localOf(int64_t epoch) { return (epoch - kDayBase) % kDay; }
};

static bool run_case_rf5_quiet_delay_does_not_consume_grace() {
  QuietHarness h;
  const int64_t due = kDayBase + 22 * kHour;  // 22:00 local
  ReminderRecord r;
  r.kind = ReminderKind::TaskDue;
  r.child_id = ChildId{"child-1"};
  r.task_id = TaskId{"task-1"};
  r.due_epoch_ms = due;
  r.instance_id = ReminderCore::makeInstanceId(r.kind, r.child_id, r.task_id, due);
  CHECK(h.core->upsert(r));

  // 22:00 local — inside the quiet window: suppressed but kept pending.
  h.setEpoch(due);
  CHECK(h.core->collectDue(QuietHarness::localOf(due)).empty());
  CHECK(h.core->activeCount() == 1);
  CHECK(h.core->diagnostics().expired_total == 0);

  // 06:59 next day — still quiet. The normal 5 min grace has long passed, but a
  // quiet-induced delay must NOT retire the reminder.
  const int64_t pre_0700 = kDayBase + kDay + 6 * kHour + 59 * 60 * 1000;
  h.setEpoch(pre_0700);
  CHECK(h.core->collectDue(QuietHarness::localOf(pre_0700)).empty());
  CHECK(h.core->activeCount() == 1);
  CHECK(h.core->diagnostics().expired_total == 0);

  // 07:00 — the next allowed window performs exactly one bounded catch-up.
  const int64_t at_0700 = kDayBase + kDay + 7 * kHour;
  h.setEpoch(at_0700);
  CHECK(at_0700 - due > h.policy.grace_ms);  // plain grace would have expired
  const auto due_now = h.core->collectDue(QuietHarness::localOf(at_0700));
  CHECK(due_now.size() == 1);
  CHECK(due_now[0].instance_id == r.instance_id);
  return true;
}

static bool run_case_rf5_quiet_catch_up_is_bounded() {
  QuietHarness h;
  const int64_t due = kDayBase + 22 * kHour;
  ReminderRecord r;
  r.kind = ReminderKind::TaskDue;
  r.child_id = ChildId{"child-1"};
  r.task_id = TaskId{"task-1"};
  r.due_epoch_ms = due;
  r.instance_id = ReminderCore::makeInstanceId(r.kind, r.child_id, r.task_id, due);
  CHECK(h.core->upsert(r));

  h.setEpoch(due);
  CHECK(h.core->collectDue(QuietHarness::localOf(due)).empty());  // quiet

  // First allowed observation opens the bounded catch-up window.
  const int64_t at_0700 = kDayBase + kDay + 7 * kHour;
  h.setEpoch(at_0700);
  CHECK(h.core->collectDue(QuietHarness::localOf(at_0700)).size() == 1);
  // The caller never presented / marked it.
  const int64_t after_window =
      at_0700 + h.policy.quiet_grace_ms + 60 * 1000;
  h.setEpoch(after_window);
  CHECK(h.core->collectDue(QuietHarness::localOf(after_window)).empty());
  CHECK(h.core->activeCount() == 0);                 // retired
  CHECK(h.core->diagnostics().expired_total == 1);   // no infinite backfill
  return true;
}

static bool run_case_rf5_schedule_refresh_preserves_snooze() {
  Harness h;
  CHECK(h.trustTime());
  const ReminderRecord r =
      makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch + 60'000);
  CHECK(h.core->upsert(r));
  CHECK(h.core->snooze(r.instance_id, kBaseEpoch));
  const int64_t snoozed_to = h.core->records()[0].snooze_until_epoch_ms;
  CHECK(snoozed_to == kBaseEpoch + h.policy.snooze_ms);

  // The plan is rebuilt and the SAME stable reminder identity is refreshed with
  // a record whose lifecycle fields are zero.
  const ReminderRecord refresh =
      makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch + 60'000);
  CHECK(h.core->upsert(refresh));
  CHECK(h.core->records().size() == 1);              // no duplicate instance
  CHECK(h.core->records()[0].snooze_until_epoch_ms == snoozed_to);  // preserved

  // Delivered state is preserved the same way.
  CHECK(h.core->markDelivered(r.instance_id, kBaseEpoch));
  CHECK(h.core->upsert(refresh));
  CHECK(h.core->records()[0].delivered_epoch_ms == kBaseEpoch);

  // An EXPLICIT reset does wipe the lifecycle.
  ReminderRecord reset = refresh;
  reset.reset_lifecycle = true;
  CHECK(h.core->upsert(reset));
  CHECK(h.core->records()[0].snooze_until_epoch_ms == 0);
  CHECK(h.core->records()[0].delivered_epoch_ms == 0);
  return true;
}

static bool run_case_rf5_capacity_bounds_persisted_records() {
  Harness h;
  h.policy.max_instances = 4;
  h.policy.max_records = 6;
  h.build();
  CHECK(h.trustTime());

  // Four reminders that are delivered and therefore finished.
  for (int i = 0; i < 4; ++i) {
    const auto rec = makeRecord(ReminderKind::TaskDue,
                                "task-" + std::to_string(i), kBaseEpoch + i);
    CHECK(h.core->upsert(rec));
    CHECK(h.core->markDelivered(rec.instance_id, kBaseEpoch));
  }
  CHECK(h.core->recordCount() == 4);  // finished rows are kept, but bounded

  // Four more LIVE reminders: finished rows are GC'd to respect max_records.
  for (int i = 4; i < 8; ++i) {
    CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue,
                                    "task-" + std::to_string(i), kBaseEpoch + i)));
  }
  CHECK(h.core->recordCount() <= h.policy.max_records);
  CHECK(h.core->diagnostics().persisted_records <= h.policy.max_records);
  CHECK(h.core->activeCount() == 4);           // pending rows never GC'd
  CHECK(h.core->diagnostics().pruned_total >= 2);

  // The ACTIVE ceiling is still refused explicitly rather than silently.
  CHECK(!h.core->upsert(makeRecord(ReminderKind::TaskDue, "task-x",
                                   kBaseEpoch + 99'999)));
  CHECK(h.core->diagnostics().rejected_capacity >= 1);
  // Reminder capacity is its own budget: it never touches the learning outbox.
  CHECK(h.core->activeCount() == 4);
  return true;
}

static bool run_case_rf5_multiple_due_reminders_merge_into_one_batch() {
  Harness h;
  // A 30 s merge window: three reminders a second apart collapse into ONE batch,
  // a fourth one a minute earlier stays its own group. No clock jump is needed
  // (the authority's holdover window stays valid).
  h.policy.merge_window_ms = 30 * 1000;
  h.policy.max_batches_per_tick = 2;
  h.build();
  CHECK(h.trustTime());

  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "t-a", kBaseEpoch - 60'000)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "t-b", kBaseEpoch - 59'500)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "t-c", kBaseEpoch - 59'100)));
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "t-d", kBaseEpoch)));

  const auto batches = h.core->collectDueBatches(kNoon);
  CHECK(batches.size() == 2);           // merged group + separate reminder
  CHECK(batches[0].items.size() == 3);  // the three close ones collapse into ONE
  CHECK(batches[1].items.size() == 1);
  CHECK(h.core->diagnostics().merged_batches == 1);
  // The single-reminder helper still agrees on the due set.
  CHECK(h.core->collectDue(kNoon).size() == 2);  // capped by max_deliveries_per_tick
  return true;
}

static bool run_case_rf5_power_loss_recovery_semantics() {
  FakeClock clock;
  TimeAuthority authority(
      clock, "boot-p", TimeAuthorityConfig{60000, 600000, 60000, 100});
  CHECK(authority.acceptSync(kBaseEpoch, "net", 1000).outcome ==
        claw4::time::SyncOutcome::Accepted);
  FakeReminderStore store;
  ReminderPolicy policy;

  const ReminderRecord r =
      makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch - 1000);

  // (1) collectDue happened, presentation did NOT, and we crash before it:
  //     the reminder is still undelivered and must fire again after restart.
  {
    ReminderCore core(policy, store, authority);
    CHECK(core.upsert(r));
    CHECK(core.collectDue(kNoon).size() == 1);
  }
  {
    ReminderCore core2(policy, store, authority);
    CHECK(core2.load());
    CHECK(core2.collectDue(kNoon).size() == 1);   // bounded re-fire, not lost
    // (2) Presented but crashing BEFORE markDelivered commits: the same
    //     bounded duplication is allowed (documented, not exactly-once).
    CHECK(core2.markDelivered(r.instance_id, kBaseEpoch));
  }
  {
    // (3) markDelivered was committed before the crash: never fires again.
    ReminderCore core3(policy, store, authority);
    CHECK(core3.load());
    CHECK(core3.collectDue(kNoon).empty());
    CHECK(core3.diagnostics().delivered_total == 1);
  }
  return true;
}

static bool run_case_rf5_wake_port_scheduled_updated_and_cancelled() {
  Harness h;
  CHECK(h.trustTime());
  FakeReminderWakePort wake;

  // Nothing pending: the platform wake is cancelled, not left armed.
  CHECK(!h.core->syncWake(wake, kNoon));
  CHECK(wake.cancel_calls == 1);
  CHECK(!wake.wake_armed);

  // A future reminder arms a wake at its due monotonic instant.
  const ReminderRecord r =
      makeRecord(ReminderKind::TaskDue, "task-1", kBaseEpoch + 60'000);
  CHECK(h.core->upsert(r));
  CHECK(h.core->syncWake(wake, kNoon));
  CHECK(wake.wake_armed);
  CHECK(wake.scheduled_deadline_ms == h.clock.mono + 60'000);

  // Snoozing pushes the deadline out.
  CHECK(h.core->snooze(r.instance_id, kBaseEpoch));
  CHECK(h.core->syncWake(wake, kNoon));
  CHECK(wake.scheduled_deadline_ms == h.clock.mono + h.policy.snooze_ms);

  // Cancelling the reminder (reschedule / delete / complete) cancels the wake.
  CHECK(h.core->cancelForTask(TaskId{"task-1"}));
  CHECK(!h.core->syncWake(wake, kNoon));
  CHECK(!wake.wake_armed);
  CHECK(wake.cancel_calls == 2);
  return true;
}

// ---------------------------------------------------------------------------
// REVIEW-FIX-002 R8: the platform wake must obey the SAME calendar/quiet policy
// as delivery — otherwise a device would wake into a reminder it may not
// present (and, with a due-but-quiet reminder, re-arm an immediate wake,
// producing a wake storm).
// ---------------------------------------------------------------------------

// R8.1: no wake may be armed while the authority forbids calendar work.
static bool run_case_stale_or_untrusted_calendar_does_not_arm_wake() {
  Harness h;
  FakeReminderWakePort wake;
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "task-1",
                                  kBaseEpoch + 60'000)));

  // (a) Unsynced: epoch_valid and calendar_allowed are both false.
  CHECK(!h.core->nextWakeMonotonicMs(kNoon).has_value());
  CHECK(!h.core->syncWake(wake, kNoon));
  CHECK(wake.cancel_calls == 1);
  CHECK(!wake.wake_armed);

  // (b) Synced, but the holdover window has expired: the epoch stays derivable
  //     while calendar work is refused.
  CHECK(h.trustTime());
  CHECK(h.core->nextWakeMonotonicMs(kNoon).has_value());  // trusted -> armed
  h.clock.mono = 700'000;                                 // > max_holdover_ms
  const auto stale = h.authority.status();
  CHECK(stale.epoch_valid);            // an epoch is still derivable...
  CHECK(!stale.calendar_allowed);      // ...but no calendar work is allowed
  CHECK(!h.core->nextWakeMonotonicMs(kNoon).has_value());
  CHECK(!h.core->syncWake(wake, kNoon));
  CHECK(wake.cancel_calls == 2);

  // (c) Epoch valid AND inside the holdover window, but the calendar
  //     uncertainty exceeds the policy cap: still no wake.
  {
    FakeClock clock;
    TimeAuthority auth(clock, "boot-u",
                       TimeAuthorityConfig{60000, 600000, 60000, 100});
    CHECK(auth.acceptSync(kBaseEpoch, "net", 10'000'000).outcome ==
          claw4::time::SyncOutcome::Accepted);
    FakeReminderStore store;
    ReminderPolicy policy;
    ReminderCore core(policy, store, auth);
    CHECK(core.upsert(makeRecord(ReminderKind::TaskDue, "task-1",
                                 kBaseEpoch + 60'000)));
    const auto uncertain = auth.status();
    CHECK(uncertain.epoch_valid);
    CHECK(uncertain.holdover_window);
    CHECK(uncertain.uncertainty_ms.has_value());
    CHECK(*uncertain.uncertainty_ms > 60000);
    CHECK(!uncertain.calendar_allowed);
    CHECK(!core.nextWakeMonotonicMs(kNoon).has_value());
  }
  return true;
}

// R8.1: an ALREADY armed wake is cancelled the moment trust is lost, so the
// platform is never left waiting for an instant the core cannot act on.
static bool run_case_wake_cancelled_when_calendar_not_allowed() {
  Harness h;
  CHECK(h.trustTime());
  FakeReminderWakePort wake;
  CHECK(h.core->upsert(makeRecord(ReminderKind::TaskDue, "task-1",
                                  kBaseEpoch + 60'000)));
  CHECK(h.core->syncWake(wake, kNoon));
  CHECK(wake.wake_armed);
  const int scheduled_before = wake.schedule_calls;

  h.clock.mono = 700'000;  // holdover expired -> calendar work refused
  CHECK(!h.core->syncWake(wake, kNoon));
  CHECK(wake.cancel_calls == 1);
  CHECK(!wake.wake_armed);
  CHECK(wake.schedule_calls == scheduled_before);  // never re-armed
  return true;
}

// R8.2: a reminder that is due INSIDE the quiet period must wake at quiet_end,
// not immediately (otherwise: wake -> suppressed -> immediate wake -> storm).
static bool run_case_quiet_deferred_reminder_wakes_at_quiet_end() {
  QuietHarness h;
  const int64_t due = kDayBase + 22 * kHour;  // 22:00 local, inside quiet
  ReminderRecord r;
  r.kind = ReminderKind::TaskDue;
  r.child_id = ChildId{"child-1"};
  r.task_id = TaskId{"task-1"};
  r.due_epoch_ms = due;
  r.instance_id = ReminderCore::makeInstanceId(r.kind, r.child_id, r.task_id, due);
  CHECK(h.core->upsert(r));

  h.setEpoch(due);
  const int64_t lod = QuietHarness::localOf(due);
  CHECK(lod == 22 * kHour);
  CHECK(h.core->collectDue(lod).empty());  // suppressed by quiet
  CHECK(h.core->activeCount() == 1);       // ...but kept pending

  // quiet_start (21:00) has passed, so quiet_end is 07:00 of the NEXT day:
  // 22:00 + 9 h = 31 h in monotonic milliseconds.
  FakeReminderWakePort wake;
  CHECK(h.core->syncWake(wake, lod));
  CHECK(wake.wake_armed);
  CHECK(wake.scheduled_deadline_ms == 31 * kHour);
  CHECK(wake.scheduled_deadline_ms > h.clock.mono);  // never immediate
  return true;
}

// R8.2: before quiet_end the period ends on the SAME local day.
static bool run_case_quiet_after_midnight_wakes_same_day_quiet_end() {
  QuietHarness h;
  const int64_t due = kDayBase + kDay + 30 * 60 * 1000;  // 00:30 local
  ReminderRecord r;
  r.kind = ReminderKind::TaskDue;
  r.child_id = ChildId{"child-1"};
  r.task_id = TaskId{"task-1"};
  r.due_epoch_ms = due;
  r.instance_id = ReminderCore::makeInstanceId(r.kind, r.child_id, r.task_id, due);
  CHECK(h.core->upsert(r));

  h.setEpoch(due);
  const int64_t lod = QuietHarness::localOf(due);
  CHECK(lod == 30 * 60 * 1000);
  CHECK(h.core->collectDue(lod).empty());

  // 00:30 -> 07:00 of the day already begun = 6.5 h, i.e. 31 h monotonic.
  FakeReminderWakePort wake;
  CHECK(h.core->syncWake(wake, lod));
  CHECK(wake.wake_armed);
  CHECK(wake.scheduled_deadline_ms == 31 * kHour);
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
  // REVIEW-FIX-001 RF5
  CASE(rf5_quiet_delay_does_not_consume_grace);
  CASE(rf5_quiet_catch_up_is_bounded);
  CASE(rf5_schedule_refresh_preserves_snooze);
  CASE(rf5_capacity_bounds_persisted_records);
  CASE(rf5_multiple_due_reminders_merge_into_one_batch);
  CASE(rf5_power_loss_recovery_semantics);
  CASE(rf5_wake_port_scheduled_updated_and_cancelled);
  // REVIEW-FIX-002 R8
  CASE(stale_or_untrusted_calendar_does_not_arm_wake);
  CASE(wake_cancelled_when_calendar_not_allowed);
  CASE(quiet_deferred_reminder_wakes_at_quiet_end);
  CASE(quiet_after_midnight_wakes_same_day_quiet_end);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 CP4 reminder core (B03) tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
