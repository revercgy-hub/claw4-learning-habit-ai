// claw4/firmware/main/reminder/reminder_core.cpp
#include "reminder/reminder_core.h"

#include <algorithm>
#include <string>

namespace claw4 {
namespace reminder {
namespace {

constexpr int64_t kDayMs = 24 * 3600 * 1000;

}  // namespace

ReminderCore::ReminderCore(ReminderPolicy policy, ReminderStore& store,
                           claw4::time::TimeAuthority& time_authority)
    : policy_(policy), store_(store), time_authority_(time_authority) {}

// R8.2: one definition of "is this local time inside the quiet period", used by
// BOTH delivery (evaluate) and wake scheduling (nextWakeMonotonicMs).
bool ReminderCore::quietActive(const int64_t local_ms_of_day) const {
  const int64_t qs = policy_.quiet_start_ms_of_day;
  const int64_t qe = policy_.quiet_end_ms_of_day;
  if (qs == qe) return false;  // zero-length quiet period
  if (qs < qe) return local_ms_of_day >= qs && local_ms_of_day < qe;
  // Wraps midnight (the default 21:00 -> 07:00 window).
  return local_ms_of_day >= qs || local_ms_of_day < qe;
}

int64_t ReminderCore::quietEndDeltaMs(const int64_t local_ms_of_day) const {
  const int64_t qe = policy_.quiet_end_ms_of_day;
  // Before quiet_end: the period ends later on the SAME local day.
  // At/after quiet_end: the period started yesterday, so it ends the NEXT day.
  return local_ms_of_day < qe ? qe - local_ms_of_day
                              : (qe + kDayMs) - local_ms_of_day;
}

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

int64_t ReminderCore::effectiveDue(const ReminderRecord& r) {
  return r.snooze_until_epoch_ms > 0 ? r.snooze_until_epoch_ms : r.due_epoch_ms;
}

bool ReminderCore::isFinished(const ReminderRecord& r) {
  return !r.active || r.delivered_epoch_ms != 0;
}

void ReminderCore::recount() {
  int active = 0;
  for (const auto& r : records_) {
    if (r.active) ++active;
  }
  diagnostics_.active_instances = active;
  diagnostics_.persisted_records = static_cast<int>(records_.size());
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

bool ReminderCore::pruneFinishedIfNeeded(std::vector<ReminderRecord>& records) {
  // RF5.3: `max_instances` bounds the ACTIVE set; `max_records` bounds what is
  // actually persisted, so finished/inactive rows cannot accumulate forever.
  while (static_cast<int>(records.size()) > policy_.max_records) {
    int victim = -1;
    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
      if (!isFinished(records[static_cast<std::size_t>(i)])) continue;
      if (victim < 0) {
        victim = i;
        continue;
      }
      const ReminderRecord& candidate = records[static_cast<std::size_t>(i)];
      const ReminderRecord& current = records[static_cast<std::size_t>(victim)];
      const bool candidate_inactive = !candidate.active;
      const bool current_inactive = !current.active;
      if (candidate_inactive != current_inactive) {
        if (candidate_inactive) victim = i;
        continue;
      }
      if (effectiveDue(candidate) < effectiveDue(current)) victim = i;
    }
    if (victim < 0) return false;  // only pending records left: refuse
    records.erase(records.begin() + victim);
    ++diagnostics_.pruned_total;
  }
  return true;
}

bool ReminderCore::load() {
  std::vector<ReminderRecord> loaded;
  if (!store_.load(loaded)) return false;
  records_ = std::move(loaded);
  // A rebuild preserves snooze/delivered/deferred marks exactly; only the
  // counters reset.
  diagnostics_ = ReminderDiagnostics{};
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
    if (r.instance_id != record.instance_id) continue;
    if (record.reset_lifecycle) {
      r = record;  // explicit reset requested by the caller
    } else {
      // RF5.2: refreshing the SCHEDULE of the same stable reminder identity
      // must not wipe the lifecycle: snooze, delivered/ack and quiet catch-up.
      ReminderRecord merged = record;
      if (record.snooze_until_epoch_ms == 0) {
        merged.snooze_until_epoch_ms = r.snooze_until_epoch_ms;
      }
      if (record.delivered_epoch_ms == 0) {
        merged.delivered_epoch_ms = r.delivered_epoch_ms;
      }
      merged.deferred_by_quiet = r.deferred_by_quiet;
      merged.catch_up_deadline_epoch_ms = r.catch_up_deadline_epoch_ms;
      merged.active = r.active;
      r = merged;
    }
    return commit(std::move(next));
  }

  // New instance: bounded ACTIVE set first.
  int active = 0;
  for (const auto& r : next) {
    if (r.active) ++active;
  }
  if (active >= policy_.max_instances) {
    ++diagnostics_.rejected_capacity;
    return false;
  }
  next.push_back(record);
  // RF5.3: then the bounded PERSISTED set (GC finished rows, refuse if full).
  if (!pruneFinishedIfNeeded(next)) {
    ++diagnostics_.rejected_capacity;
    return false;
  }
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
    if (r.instance_id != instance_id) continue;
    r.snooze_until_epoch_ms = now_epoch_ms + policy_.snooze_ms;
    r.delivered_epoch_ms = 0;  // eligible to fire again at the new time
    r.active = true;
    // A fresh snooze supersedes any quiet catch-up bookkeeping.
    r.deferred_by_quiet = false;
    r.catch_up_deadline_epoch_ms = 0;
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
    if (r.instance_id != instance_id) continue;
    r.delivered_epoch_ms = now_epoch_ms;
    r.active = false;  // delivered => finished, no longer an active instance
    r.deferred_by_quiet = false;
    r.catch_up_deadline_epoch_ms = 0;
    found = true;
    break;
  }
  if (!found) return false;
  if (!commit(std::move(next))) return false;
  ++diagnostics_.delivered_total;
  return true;
}

// ---------------------------------------------------------------------------
// shared evaluation
// ---------------------------------------------------------------------------
ReminderCore::Evaluation ReminderCore::evaluate(const int64_t local_ms_of_day) {
  Evaluation ev;
  // Reuse the B01 authority: this module never reads the raw RTC itself.
  const time::TimeStatus status = time_authority_.status();
  diagnostics_.last_evaluation_time_trusted = status.calendar_allowed;

  // Untrusted / unusable time: NO calendar reminder may fire. This is what
  // stops a bad clock from ringing historical reminders after a jump.
  if (!status.calendar_allowed || !status.epoch_valid || !status.epoch_ms) {
    ++diagnostics_.suppressed_untrusted_time;
    return ev;
  }
  ev.usable = true;
  ev.now = *status.epoch_ms;
  ev.quiet = quietActive(local_ms_of_day);

  std::vector<ReminderRecord> next = records_;
  bool dirty = false;
  for (auto& r : next) {
    if (isFinished(r)) continue;
    const int64_t eff = effectiveDue(r);
    if (eff > ev.now) continue;  // not due yet

    if (ev.quiet) {
      // RF5.1 frozen semantics: the quiet period suppresses delivery but keeps
      // the reminder pending, and a quiet-induced delay does NOT consume the
      // normal grace window.
      if (!r.deferred_by_quiet) {
        r.deferred_by_quiet = true;
        dirty = true;
      }
      ++diagnostics_.suppressed_quiet;
      continue;
    }

    if (r.deferred_by_quiet) {
      // First allowed window after quiet: open exactly ONE bounded catch-up.
      if (r.catch_up_deadline_epoch_ms == 0) {
        r.catch_up_deadline_epoch_ms = ev.now + policy_.quiet_grace_ms;
        dirty = true;
      }
      if (ev.now <= r.catch_up_deadline_epoch_ms) {
        ev.due.push_back(r);
        continue;
      }
      // Catch-up window exhausted: retire. No infinite backfill of history.
      r.active = false;
      r.deferred_by_quiet = false;
      r.catch_up_deadline_epoch_ms = 0;
      ++diagnostics_.expired_total;
      dirty = true;
      continue;
    }

    if (ev.now - eff > policy_.grace_ms) {
      // A forward counter jump must not replay history.
      r.active = false;
      ++diagnostics_.expired_total;
      dirty = true;
      continue;
    }
    ev.due.push_back(r);
  }

  if (dirty && !commit(next)) {
    ev.due.clear();
    ev.usable = false;
    return ev;
  }

  std::stable_sort(ev.due.begin(), ev.due.end(),
                   [](const ReminderRecord& a, const ReminderRecord& b) {
                     const int64_t ea = effectiveDue(a);
                     const int64_t eb = effectiveDue(b);
                     if (ea != eb) return ea < eb;
                     return a.instance_id < b.instance_id;
                   });
  return ev;
}

std::vector<ReminderRecord> ReminderCore::collectDue(const int64_t local_ms_of_day) {
  const Evaluation ev = evaluate(local_ms_of_day);
  std::vector<ReminderRecord> deliver;
  if (!ev.usable) return deliver;
  const int limit = std::min<int>(policy_.max_deliveries_per_tick,
                                  static_cast<int>(ev.due.size()));
  for (int i = 0; i < limit; ++i) {
    deliver.push_back(ev.due[static_cast<std::size_t>(i)]);
  }
  return deliver;
}

std::vector<ReminderBatch> ReminderCore::collectDueBatches(
    const int64_t local_ms_of_day) {
  std::vector<ReminderBatch> out;
  const Evaluation ev = evaluate(local_ms_of_day);
  if (!ev.usable) return out;

  // RF5.4: merge same child + same kind + due times inside the merge window
  // into ONE group, so the caller presents a single message.
  std::vector<ReminderBatch> groups;
  for (const auto& r : ev.due) {
    bool placed = false;
    for (auto& g : groups) {
      if (g.child_id == r.child_id && g.kind == r.kind &&
          effectiveDue(r) - g.anchor_epoch_ms <= policy_.merge_window_ms) {
        g.items.push_back(r);
        placed = true;
        break;
      }
    }
    if (!placed) {
      ReminderBatch batch;
      batch.child_id = r.child_id;
      batch.kind = r.kind;
      batch.anchor_epoch_ms = effectiveDue(r);
      batch.items.push_back(r);
      groups.push_back(std::move(batch));
    }
  }

  const int limit = std::min<int>(policy_.max_batches_per_tick,
                                  static_cast<int>(groups.size()));
  for (int i = 0; i < limit; ++i) {
    if (groups[static_cast<std::size_t>(i)].items.size() > 1) {
      ++diagnostics_.merged_batches;
    }
    out.push_back(groups[static_cast<std::size_t>(i)]);
  }
  return out;
}

std::optional<int64_t> ReminderCore::nextWakeMonotonicMs(
    const int64_t local_ms_of_day) {
  const time::TimeStatus status = time_authority_.status();
  // R8.1: the wake must obey the SAME trust gate as delivery. If the authority
  // does not allow calendar work (Unsynced / holdover expired / uncertainty
  // over the calendar error cap) the core cannot present anything, so arming a
  // wake would only produce a wake that immediately suppresses everything.
  // nullopt makes the caller cancel any previously armed wake.
  if (!status.calendar_allowed || !status.epoch_valid || !status.epoch_ms) {
    return std::nullopt;
  }

  const int64_t now_epoch = *status.epoch_ms;
  bool found = false;
  int64_t best_epoch = 0;
  for (const auto& r : records_) {
    if (isFinished(r)) continue;
    int64_t candidate = effectiveDue(r);
    if (r.deferred_by_quiet && r.catch_up_deadline_epoch_ms != 0) {
      candidate = std::min(candidate, r.catch_up_deadline_epoch_ms);
    }
    if (candidate <= now_epoch) candidate = now_epoch;  // due (or overdue) now

    // R8.2: waking at an instant that is still inside the quiet period is
    // useless — the reminder would be suppressed again and the core would
    // re-arm an immediate wake, i.e. a wake storm once Light Sleep lands. Push
    // the deadline to the END of the quiet period instead.
    const int64_t delta_to_candidate = candidate - now_epoch;
    // Reduce the delta to a day offset first so the addition cannot overflow on
    // a far-future due time.
    const int64_t local_at_candidate =
        (local_ms_of_day + delta_to_candidate % kDayMs) % kDayMs;
    if (quietActive(local_at_candidate)) {
      candidate += quietEndDeltaMs(local_at_candidate);
    }

    if (!found || candidate < best_epoch) {
      best_epoch = candidate;
      found = true;
    }
  }
  if (!found) return std::nullopt;
  const int64_t delta = best_epoch - now_epoch;
  return status.monotonic_ms + (delta > 0 ? delta : 0);
}

bool ReminderCore::syncWake(claw4::ports::ReminderWakePort& wake,
                            const int64_t local_ms_of_day) {
  const auto deadline = nextWakeMonotonicMs(local_ms_of_day);
  if (!deadline) {
    wake.cancelWake();  // nothing pending: do not keep the platform awake
    return false;
  }
  return wake.scheduleWakeAt(*deadline);
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
