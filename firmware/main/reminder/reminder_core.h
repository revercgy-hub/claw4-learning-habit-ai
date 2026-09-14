// claw4/firmware/main/reminder/reminder_core.h
// WB-V53-NEXT-001 CP4 (B03): reminder scheduling + persistence recovery core.
//
// Pure Host logic (no esp_sleep / LVGL / BSP). It reuses the B01 TimeAuthority
// status instead of creating a second clock: calendar work is only allowed when
// the authority says so.
//
// Contracts enforced here (work package B03):
//   * stable instance ids, so a rebuild/reconfigure never duplicates a reminder;
//   * a rebuild does NOT reset an existing Snooze;
//   * reschedule / delete / complete cancels the old reminders for that task;
//   * TaskDue / Snooze / FocusEnd / DailyReview are all supported;
//   * a counter jump (forward or backward) never replays history: reminders
//     whose grace window has passed are retired instead of ringing late;
//   * untrusted time (Unsynced / Stale / authority not allowing calendar work)
//     suppresses calendar reminders entirely;
//   * the quiet period suppresses delivery but keeps the reminder pending;
//   * at most `max_deliveries_per_tick` reminders are handed out per evaluation;
//   * the instance table is bounded by `max_instances` and it is a SEPARATE
//     budget from the learning outbox, so reminder telemetry can never starve
//     the critical learning events;
//   * a save failure rolls the in-memory change back (no half-applied state).
//
// Not in scope here: the physical ring's exactly-once guarantee, and the device
// adapters (C03).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "learning_domain/ids.h"
#include "ports/reminder_wake_port.h"
#include "time/time_authority.h"

namespace claw4 {
namespace reminder {

enum class ReminderKind : uint8_t {
  TaskDue = 0,
  Snooze,
  FocusEnd,
  DailyReview,
};

struct ReminderRecord {
  std::string instance_id;                 // stable, derived from the identity
  ReminderKind kind = ReminderKind::TaskDue;
  claw4::domain::ChildId child_id;
  claw4::domain::TaskId task_id;
  int64_t due_epoch_ms = 0;
  int64_t snooze_until_epoch_ms = 0;       // 0 => not snoozed
  int64_t delivered_epoch_ms = 0;          // 0 => not presented yet
  bool active = true;
  // RF5.1: set while the reminder was suppressed by the quiet period. A
  // quiet-induced delay does NOT consume the normal grace window; the core
  // grants exactly one bounded catch-up in the next allowed window.
  bool deferred_by_quiet = false;
  int64_t catch_up_deadline_epoch_ms = 0;  // 0 => no catch-up in progress
  // RF5.2: explicit opt-in to wipe lifecycle state on a same-id upsert.
  bool reset_lifecycle = false;
};

struct ReminderPolicy {
  int max_instances = 16;              // bound on ACTIVE instances
  int max_records = 32;                // RF5.3: bound on PERSISTED records
  int max_deliveries_per_tick = 2;
  int max_batches_per_tick = 1;        // RF5.4: merged groups handed out per tick
  int64_t grace_ms = 5 * 60 * 1000;              // 5 min late-acceptance window
  int64_t quiet_grace_ms = 5 * 60 * 1000;        // RF5.1 bounded catch-up window
  int64_t merge_window_ms = 5 * 60 * 1000;       // RF5.4 same-child merge window
  int64_t snooze_ms = 10 * 60 * 1000;            // 10 min snooze
  int64_t quiet_start_ms_of_day = 21 * 3600 * 1000;  // 21:00 local
  int64_t quiet_end_ms_of_day = 7 * 3600 * 1000;     // 07:00 local
};

// RF5.4: a merged group the caller presents as ONE message instead of a burst.
struct ReminderBatch {
  claw4::domain::ChildId child_id;
  ReminderKind kind = ReminderKind::TaskDue;
  int64_t anchor_epoch_ms = 0;             // earliest effective due time
  std::vector<ReminderRecord> items;
};

struct ReminderDiagnostics {
  int active_instances = 0;
  int persisted_records = 0;
  int delivered_total = 0;
  int expired_total = 0;
  int rejected_capacity = 0;
  int pruned_total = 0;                  // RF5.3 GC of finished records
  int suppressed_quiet = 0;
  int suppressed_untrusted_time = 0;
  int save_failures = 0;
  int merged_batches = 0;
  bool last_evaluation_time_trusted = false;
};

// Injectable persistence. A save failure must leave the previous content
// visible (the core then rolls its in-memory change back).
class ReminderStore {
 public:
  virtual ~ReminderStore() = default;
  virtual bool load(std::vector<ReminderRecord>& out) = 0;
  virtual bool save(const std::vector<ReminderRecord>& records) = 0;
};

class ReminderCore {
 public:
  ReminderCore(ReminderPolicy policy, ReminderStore& store,
               claw4::time::TimeAuthority& time_authority);

  // Rebuild from persistence. Existing snooze/delivered marks are preserved
  // exactly (a reboot or runtime rebuild must not reset them).
  bool load();

  // Creates or updates one reminder. Returns false (and changes nothing) when
  // the instance table is already at capacity and this is a new instance.
  bool upsert(const ReminderRecord& record);

  // Removes all reminders bound to the task (reschedule / delete / complete).
  bool cancelForTask(const claw4::domain::TaskId& task_id);
  // Removes every reminder for the child (plan replaced).
  bool cancelForChild(const claw4::domain::ChildId& child_id);

  // Snooze is deliberately idempotent on the instance, not on the task: the
  // stable instance id keeps the same reminder across rebuilds.
  bool snooze(const std::string& instance_id, int64_t now_epoch_ms);
  // Marks the instance presented so it will never fire again.
  bool markDelivered(const std::string& instance_id, int64_t now_epoch_ms);

  // Evaluation: returns the reminders that should be presented now, in due
  // order, capped by `max_deliveries_per_tick`. It reads the B01 TimeAuthority
  // (never a second clock) and only acts when the authority allows calendar
  // work. Retiring over-grace instances is the single bounded write it may
  // perform; presentation itself is the caller's job, followed by
  // markDelivered(). `local_ms_of_day` comes from the C01 timezone layer.
  std::vector<ReminderRecord> collectDue(int64_t local_ms_of_day);

  // RF5.4: the merged-group view. Same child + same kind + effective due times
  // inside `merge_window_ms` of the group anchor collapse into ONE batch, so the
  // caller presents a single message instead of a burst of reminders. At most
  // `max_batches_per_tick` groups are returned; the remainder stay pending.
  std::vector<ReminderBatch> collectDueBatches(int64_t local_ms_of_day);

  // RF5.6: monotonic deadline of the next reminder that still needs to fire, or
  // nullopt when nothing is pending.
  //
  // REVIEW-FIX-002 R8: the wake obeys the SAME trust and quiet policy as
  // delivery. When the authority does not allow calendar work the wake is NOT
  // armed (nullopt -> the caller cancels it), and a wake that would land inside
  // the quiet period is pushed to the end of that period instead of firing
  // immediately. Pure Host logic; no esp_sleep.
  std::optional<int64_t> nextWakeMonotonicMs(int64_t local_ms_of_day);
  // Applies that deadline to the platform wake port (schedule or cancel).
  // Returns true when a wake was scheduled.
  bool syncWake(claw4::ports::ReminderWakePort& wake, int64_t local_ms_of_day);

  int activeCount() const;
  int recordCount() const { return static_cast<int>(records_.size()); }
  const ReminderDiagnostics& diagnostics() const { return diagnostics_; }
  const std::vector<ReminderRecord>& records() const { return records_; }

  // Deterministic stable id: same identity + same due slot => same instance.
  static std::string makeInstanceId(ReminderKind kind,
                                    const claw4::domain::ChildId& child_id,
                                    const claw4::domain::TaskId& task_id,
                                    int64_t due_epoch_ms);
  // Stable id for a snooze of an existing instance.
  static std::string makeSnoozeInstanceId(const std::string& base_instance_id,
                                          int64_t snooze_until_epoch_ms);

 private:
  bool commit(std::vector<ReminderRecord> next);
  void recount();
  static int64_t effectiveDue(const ReminderRecord& r);
  static bool isFinished(const ReminderRecord& r);
  // RF5.3: drop finished (delivered / inactive) records so the PERSISTED total
  // stays bounded. Never touches pending rows.
  bool pruneFinishedIfNeeded(std::vector<ReminderRecord>& records);
  // Shared due-selection used by collectDue() and collectDueBatches().
  struct Evaluation {
    bool usable = false;
    bool quiet = false;
    int64_t now = 0;
    std::vector<ReminderRecord> due;
  };
  Evaluation evaluate(int64_t local_ms_of_day);
  // R8.2: quiet-period geometry, shared by evaluate() and nextWakeMonotonicMs()
  // so delivery and wake can never disagree about what "quiet" means.
  bool quietActive(int64_t local_ms_of_day) const;
  // Milliseconds from `local_ms_of_day` to the END of the current quiet period
  // (next day when we are past quiet_start; same day when we are before
  // quiet_end). Only meaningful while quietActive() is true.
  int64_t quietEndDeltaMs(int64_t local_ms_of_day) const;

  ReminderPolicy policy_;
  ReminderStore& store_;
  claw4::time::TimeAuthority& time_authority_;
  std::vector<ReminderRecord> records_;
  ReminderDiagnostics diagnostics_;
};

}  // namespace reminder
}  // namespace claw4
