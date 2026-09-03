// claw4/firmware/main/interaction/command.h
// Input-agnostic interaction command vocabulary (V4 §7, WB-LEARNING-V4 P14).
// Portable C++17. No LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / BSP includes.
//
// Touch, Voice(STT phrase) and MCP inputs all reduce to these canonical
// CommandKinds before entering the single CommandDispatcher funnel.
#pragma once

#include <cstdint>
#include <optional>

#include "learning_domain/ids.h"

namespace claw4 {
namespace interaction {

// Canonical command kinds understood by the interaction layer.
// `Query*` kinds are read-only projections handled by the MCP host / UI
// presenters directly; they NEVER enter the domain reducer as mutations.
enum class CommandKind : uint8_t {
  Unknown = 0,
  StartTask,
  PauseTask,
  ResumeTask,
  CompleteTask,
  SkipTask,
  QueryTodayTasks,
  QueryCurrentTask,
  QueryRemainingTime,
  QueryTodayProgress,
};

// Parameters accompanying a command. `task_id` is REQUIRED for every mutation:
// the domain reducer keys all transitions on the task that owns the active
// session (learning_domain/reducer.cpp). `session_id` is optional and passed
// through unchanged for callers that already resolved it.
struct CommandPayload {
  CommandKind kind = CommandKind::Unknown;
  std::optional<domain::TaskId> task_id;
  std::optional<domain::SessionId> session_id;
};

}  // namespace interaction
}  // namespace claw4
