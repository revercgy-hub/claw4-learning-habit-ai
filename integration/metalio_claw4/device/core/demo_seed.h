// claw4/integration/metalio_claw4/device/core/demo_seed.h
// WB-LEARNING-V4-L1 — first-boot demo today snapshot.
// Pure header, host-testable. NOT an authoritative task source: the device
// only caches server snapshots (ARCHITECTURE.md §2.2 / I4). Until the L2
// server sync lands, boot seeds this demo list ONCE (guarded by
// NvsOutboxStorage::hasState()); L2 replaces it with real backend snapshots.
#pragma once

#include <string>
#include <vector>

#include "learning_domain/task.h"

namespace claw4 {
namespace metalio {

// Demo "today" list shown on a device with no committed state. Marked demo so
// any downstream dashboard can distinguish it from real server data.
inline std::vector<claw4::domain::Task> DemoTodaySnapshot() {
  claw4::domain::Task a;
  a.task_id = claw4::domain::TaskId{"demo-math-001"};
  a.child_id = claw4::domain::ChildId{"child-1"};
  a.title = "口算练习";
  a.subject = "math";
  a.description = "100 以内加减法（demo 数据，L2 由服务器快照替换）";
  a.task_type = "practice";
  a.estimated_minutes = 20;
  a.priority = "high";
  a.status = claw4::domain::TaskStatus::Ready;
  a.scheduled_date = "2026-09-03";
  a.version = 1;

  claw4::domain::Task b;
  b.task_id = claw4::domain::TaskId{"demo-chinese-001"};
  b.child_id = claw4::domain::ChildId{"child-1"};
  b.title = "古诗背诵";
  b.subject = "chinese";
  b.description = "《静夜思》朗读与背诵（demo 数据，L2 由服务器快照替换）";
  b.task_type = "practice";
  b.estimated_minutes = 15;
  b.priority = "medium";
  b.status = claw4::domain::TaskStatus::Ready;
  b.scheduled_date = "2026-09-03";
  b.version = 1;

  return {a, b};
}

}  // namespace metalio
}  // namespace claw4
