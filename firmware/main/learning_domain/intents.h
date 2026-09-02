// claw4/firmware/main/learning_domain/intents.h
// User/assistant intents and intent results.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §3.5 / §3.6 / §8.1.
#pragma once

#include <cstdint>
#include <optional>

#include "learning_domain/ids.h"

namespace claw4 {
namespace domain {

// Intents emitted by the UI layer (and later by the assistant router).
// The UI never calls business methods directly (ARCHITECTURE.md §3.5 / I4).
enum class Intent : uint8_t {
  StartTask = 0,
  Pause,
  Resume,
  Complete,
  Skip,
  QueryTodayTasks,
  QueryRemainingTime,
};

// Result of an intent applied by the domain layer. `Idempotent` means the
// transition was already applied earlier (e.g. duplicate Complete click is
// rejected by the domain per ARCHITECTURE.md §4.5).
enum class IntentResult : uint8_t {
  Accepted = 0,
  RejectedInvalidState,   // state machine does not allow this transition
  Idempotent,             // already applied; no duplicate event generated
  PersistFailed,          // outbox persistence failed; state NOT committed (7.3.1)
};

struct IntentRequest {
  Intent intent = Intent::StartTask;
  std::optional<TaskId> task_id;
  std::optional<SessionId> session_id;
};

}  // namespace domain
}  // namespace claw4
