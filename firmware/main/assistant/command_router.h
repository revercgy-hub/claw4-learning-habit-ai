// claw4/firmware/main/assistant/command_router.h
// CommandRouter: maps a parsed voice command to a domain intent.
// Portable C++17. No LLM / ASR / TTS / wake-word dependencies.
// Contract source: ARCHITECTURE.md §3.6.
#pragma once

#include <optional>

#include "assistant/command.h"
#include "learning_domain/intents.h"

namespace claw4 {
namespace assistant {

// Routes a Command to a domain Intent. Implementations (added in a later
// checkpoint) must never call any AI Provider; voice processing (ASR/TTS) is
// explicitly out of MVP scope (ARCHITECTURE.md §3.6 / §9.4).
class CommandRouter {
 public:
  virtual ~CommandRouter() = default;

  // Returns nullopt when the command cannot be mapped (e.g. Unknown or when
  // required parameters are missing).
  virtual std::optional<claw4::domain::IntentRequest> route(const Command& command) const = 0;
};

}  // namespace assistant
}  // namespace claw4
