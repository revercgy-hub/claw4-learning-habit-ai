// claw4/firmware/main/learning_domain/services.h
// Pure domain service interfaces. Implementations are written in later
// checkpoints (T2 domain models); this checkpoint only fixes the boundaries.
// Portable C++17. No LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / BSP includes.
#pragma once

#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"
#include "learning_domain/intents.h"

namespace claw4 {
namespace domain {

// TaskService: pure task lifecycle transitions. Never touches storage or UI.
class TaskService {
 public:
  virtual ~TaskService() = default;
  virtual IntentResult start(const IntentRequest& request, const TaskId& task_id) = 0;
  virtual IntentResult pause(const TaskId& task_id) = 0;
  virtual IntentResult resume(const TaskId& task_id) = 0;
  virtual IntentResult complete(const TaskId& task_id) = 0;  // explicit user action only (I2)
  virtual IntentResult skip(const TaskId& task_id) = 0;
};

// SessionService: StudySession lifecycle. A task may produce multiple
// sessions (I1). Completion is explicit; auto_saved requires a user decision
// after complete-snapshot reboot recovery (CR-WB002-09).
class SessionService {
 public:
  virtual ~SessionService() = default;
  virtual IntentResult startSession(const IntentRequest& request, const TaskId& task_id) = 0;
  virtual IntentResult pauseSession(const SessionId& session_id) = 0;
  virtual IntentResult resumeSession(const SessionId& session_id) = 0;
  virtual IntentResult endSession(const SessionId& session_id, CompletionType completion_type) = 0;
};

// TimerService: focus-segment timer. On timeout it only ends/pauses the focus
// segment and prompts the user; it NEVER auto-completes the task (I2).
class TimerService {
 public:
  virtual ~TimerService() = default;
  virtual void onTick(int64_t monotonic_ms) = 0;
  virtual void onSegmentTimeout(const SessionId& session_id) = 0;
};

}  // namespace domain
}  // namespace claw4
