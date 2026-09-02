// claw4/firmware/main/learning_domain/study_session.h
// StudySession value type.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §4.4 / §5.2.
#pragma once

#include <cstdint>

#include "learning_domain/ids.h"
#include "learning_domain/task.h"

namespace claw4 {
namespace domain {

// Session lifecycle status. `completion_type` is a separate dimension that
// describes HOW the session ended and never replaces `status`
// (ARCHITECTURE.md §4.4 / CR-WB002-03/09).
enum class SessionStatus : uint8_t {
  Created = 0,
  Running,
  Paused,
  Completed,
  Aborted,
};

// How a session ended. There is intentionally NO `Timeout` value: a focus
// segment timeout only ends/pauses the segment and prompts the user; it never
// auto-completes the session or the task (ARCHITECTURE.md §4.4, I2).
// `ParentConfirmed` is V1, out of MVP.
enum class CompletionType : uint8_t {
  Normal = 0,    // user ended normally
  Manual,        // user ended manually
  AutoSaved,     // snapshot complete; user explicitly chose to save/end after reboot recovery
  Aborted,       // snapshot corrupted; only recoverable metadata kept
};

struct StudySession {
  SessionId session_id;
  TaskId task_id;
  ChildId child_id;
  DeviceId device_id;
  int planned_minutes = 0;
  int64_t actual_seconds = 0;   // accumulated by monotonic clock, excludes pauses
  int pause_count = 0;
  int64_t pause_seconds = 0;
  SessionStatus status = SessionStatus::Created;
  CompletionType completion_type = CompletionType::Normal;
};

}  // namespace domain
}  // namespace claw4
