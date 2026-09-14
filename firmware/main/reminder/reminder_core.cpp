// claw4/firmware/main/reminder/reminder_core.cpp
#include "reminder/reminder_core.h"

#include <algorithm>
#include <string>

namespace claw4 {
namespace reminder {
namespace {

bool sameInstance(const ReminderRecord& r, const std::string& id) {
  return r.instance_id == id;
}

}  // namespace

ReminderCore::ReminderCore(ReminderPolicy policy, ReminderStore& store,
                           claw4::time::TimeAuthority& time_authority)
    : policy_(policy), store_(store), time_authority_(time_authority) {}

std::string ReminderCore::makeInstanceId(ReminderKind kind,
                                         const claw4::domain::ChildId& child_id,
                                         const claw4::domain::TaskId& task_id,
                                         int64_t due_epoch_ms) {
  // Deterministic: the same logical reminder always maps to the same id, so a
  // rebuild/reconfigure deduplicates instead of creating a second instance.
  return "rem:" + std::to_string(static_cast<int>(kind)) + ":" +
         child_id.value + ":" + task_id.value + ":" +
         std::to_string(due_epoch_ms);
}

std::string ReminderCore::makeSnoozeInstanceId(const std::string& base_instance_id,
                                               int64_t snooze_until_epoch_ms) {
  return base_instance_id + ":snooze:" + std::to_string(snooze_until_epoch_ms);
}

void ReminderCore::recount() {
  int active = 0;
  for (const auto& r : records_) {
    if (r.active) ++active;
  }
  diagnostics_.active_instances = active;
}

ReminderRecord* ReminderCore::find(const std::string& instance_id) {
  for (auto& r : records_) {
    if (sameInstance(r, instance_id)) return &r;
  }
  return nullptr;
}

bool ReminderCore::commit(std::vector<ReminderRecord> next) {
  if (!store_.save(next)) {
    // Save failure: the previous content stays visible and our in-memory
    // records are NOT advanced (no half-applied state).
    ++diagnostics_.save_failures;
    return false;
  }
  records_ = std::move(next);
  recount();
  return true;
}

bool ReminderCore::load() {
  std::vector<ReminderRecord> loaded;
  if (!store_.load(loaded)) return false;
  records_ = std::move(loaded);
  // A rebuild preserves snooze/delivered marks exactly; only counters reset.
  diagnostics_.active_instances = 0;
  diagnostics_.delivered_total = 0;
  diagnostics_.expired_total = 0;
  diagnostics_.rejected_capacity = 0;
  diagnostics_.suppressed_quiet = 0;
  diagnostics_.suppressed_untrusted_time = 0;
  for (const auto& r : records_) {
    if (r.delivered_epoch_ms != 0) ++diagnostics_.delivered_total;
  }
  recount();
  return true;
}

bool ReminderCore::upsert(const ReminderRecord& record) {
  if (record.instance_id.empty()) return false;
  std::vector<ReminderRecord> next = records_;
  for (auto& r : next) {
    if (sameInstance(r, record.instance_id)) {
      r = record;  // update in place: identity is the instance id
      return commit(std::move(next));
    }
  }
  // New instance: bounded table. Reminder capacity is its OWN budget and can
  // never consume the learning outbox budget.
  int active = 0;
  for (const auto& r : next) {
    if (r.active) ++active;
  }
  if (active >= policy_.max_instances) {
    ++diagnostics_.rejected_capacity;
    return false;
  }
  next.push_back(record);
  return commit(std::move(next));
}

bool ReminderCore::cancelForTask(const claw4::domain::TaskId& task_id) {
  std::vector<ReminderRecord> next;
  next.reserve(records_.size());
  bool changed = false;
  for (const auto& r : records_) {
    if (r.task_id == task_id) {
      changed = true;
      continue;  // reschedule / delete / complete cancels the old reminders
    }
    next.push_back(r);
  }
  if (!changed) return true;  // nothing to do, and nothing to persist
  return commit(std::move(next));
}

bool ReminderCore::cancelForChild(const claw4::domain::ChildId& child_id) {
  std::vector<ReminderRecord> next;
  next.reserve(records_.size());
  bool changed = false;
  for (const auto& r : records_) {
    if (r.child_id == child_id) {
      changed = true;
      continue;
    }
    next.push_back(r);
  }
  if (!changed) return true;
  return commit(std::move(next));
}

bool ReminderCore::snooze(const std::string& instance_id,
                          const int64_t now_epoch_ms) {
  std::vector<ReminderRecord> next = records_;
  bool found = false;
  for (auto& r : next) {
    if (!sameInstance(r, instance_id)) continue;
    r.snooze_until_epoch_ms = now_epoch_ms + policy_.snooze_ms;
    r.delivered_epoch_ms = 0;  // eligible to fire again at the new time
    r.active = true;
    found = true;
    break;
  }
  if (!found) return false;
  return commit(std::move(next));
}

bool ReminderCore::markDelivered(const std::string& instance_id,
                                 const int64_t now_epoch_ms) {
  std::vector<ReminderRecord> next = records_;
  bool found = false;
  for (auto& r : next) {
    if (!sameInstance(r, instance_id)) continue;
    r.delivered_epoch_ms = now_epoch_ms;
    found = true;
    break;
  }
  if (!found) return false;
  if (!commit(std::move(next))) return false;
  ++diagnostics_.delivered_total;
  return true;
}

std::vector<ReminderRecord> ReminderCore::collectDue(
    const int64_t local_ms_of_day) {
  std::vector<ReminderRecord> deliver;
  // Reuse the B01 authority: this module never reads the raw RTC itself.
  const time::TimeStatus status = time_authority_.status();
  diagnostics_.last_evaluation_time_trusted = status.calendar_allowed;

  // Untrusted / unusable time: NO calendar reminder may fire. This is what
  // stops a bad clock from ringing historical reminders after a jump.
  if (!status.calendar_allowed || !status.epoch_valid || !status.epoch_ms) {
    ++diagnostics_.suppressed_untrusted_time;
    return deliver;
  }
  const int64_t now = *status.epoch_ms;

  // Pass 1 (bounded mutation): retire reminders whose grace window has passed.
  // A forward counter jump must not replay history.
  std::vector<ReminderRecord> next = records_;
  bool retired = false;
  for (auto& r : next) {
    if (!r.active || r.delivered_epoch_ms != 0) continue;
    const int64_t effective =
        r.snooze_until_epoch_ms > 0 ? r.snooze_until_epoch_ms : r.due_epoch_ms;
    if (now - effective > policy_.grace_ms) {
      r.active = false;
      ++diagnostics_.expired_total;
      retired = true;
    }
  }
  if (retired && !commit(next)) return deliver;  // save failed: report nothing

  // Quiet period: suppressed but kept pending for the next allowed window.
  const bool quiet = local_ms_of_day >= policy_.quiet_start_ms_of_day ||
                     local_ms_of_day < policy_.quiet_end_ms_of_day;
  if (quiet) {
    ++diagnostics_.suppressed_quiet;
    return deliver;
  }

  // Pass 2 (pure): collect the currently due reminders in due order.
  std::vector<const ReminderRecord*> due;
  for (const auto& r : next) {
    if (!r.active || r.delivered_epoch_ms != 0) continue;
    const int64_t effective =
        r.snooze_until_epoch_ms > 0 ? r.snooze_until_epoch_ms : r.due_epoch_ms;
    if (effective > now) continue;
    if (now - effective > policy_.grace_ms) continue;  // retired above
    due.push_back(&r);
  }
  std::stable_sort(due.begin(), due.end(),
                   [](const ReminderRecord* a, const ReminderRecord* b) {
                     const int64_t ea = a->snooze_until_epoch_ms > 0
                                            ? a->snooze_until_epoch_ms
                                            : a->due_epoch_ms;
                     const int64_t eb = b->snooze_until_epoch_ms > 0
                                            ? b->snooze_until_epoch_ms
                                            : b->due_epoch_ms;
                     if (ea != eb) return ea < eb;
                     return a->instance_id < b->instance_id;
                   });
  const int limit = std::min<int>(policy_.max_deliveries_per_tick,
                                  static_cast<int>(due.size()));
  for (int i = 0; i < limit; ++i) deliver.push_back(*due[static_cast<std::size_t>(i)]);
  return deliver;
}

int ReminderCore::activeCount() const {
  int active = 0;
  for (const auto& r : records_) {
    if (r.active) ++active;
  }
  return active;
}

}  // namespace reminder
}  // namespace claw4
