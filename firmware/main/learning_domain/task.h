// claw4/firmware/main/learning_domain/task.h
// Task value type (read-only cache copy on device).
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §4.3 / §5.1.
#pragma once

#include <cstdint>
#include <string>

#include "learning_domain/ids.h"

namespace claw4 {
namespace domain {

// Task lifecycle status (ARCHITECTURE.md §5.1).
enum class TaskStatus : uint8_t {
  Pending = 0,
  Ready,
  InProgress,
  Paused,
  Completed,
  Skipped,
};

// Read-only snapshot of a Task as cached on the device.
// The device never creates authoritative tasks; it only renders/caches the
// server-provided copy (ARCHITECTURE.md §2.2 / I4).
struct Task {
  TaskId task_id;
  ChildId child_id;
  std::string title;
  std::string subject;             // math/chinese/english/science/other
  std::string description;
  std::string task_type;           // practice / ...
  int estimated_minutes = 0;       // planned duration in minutes
  std::string priority;            // high/medium/low
  TaskStatus status = TaskStatus::Pending;
  std::string scheduled_date;      // ISO date (local timezone)
  int64_t version = 0;             // optimistic concurrency version
};

}  // namespace domain
}  // namespace claw4
