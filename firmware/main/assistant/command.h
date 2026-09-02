// claw4/firmware/main/assistant/command.h
// Voice/command vocabulary for the assistant boundary.
// Portable C++17. No LLM / ASR / TTS dependencies.
// Contract source: ARCHITECTURE.md §3.6 / 项目总规划 §十七.
#pragma once

#include <cstdint>

namespace claw4 {
namespace assistant {

// MVP command set (intent commands, NOT free-form LLM chat).
// Routing happens locally; no AI provider is contacted (ARCHITECTURE.md §3.6).
enum class Command : uint8_t {
  StartTask = 0,
  PauseTask,
  ResumeTask,
  CompleteTask,
  QueryTodayTasks,
  QueryRemainingTime,
  Unknown,
};

}  // namespace assistant
}  // namespace claw4
