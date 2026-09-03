// claw4/firmware/main/interaction/dispatcher.h
// Single interaction funnel (V4 §7, WB-LEARNING-V4 P14).
// Portable C++17. No LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / BSP includes.
//
// Every Touch / Voice / MCP input that mutates learning state enters the
// domain through this one dispatcher. The dispatcher is a PURE policy layer:
//   - it maps a canonical CommandKind to a domain IntentRequest,
//   - it enforces the completion gate: only a Touch source may emit
//     Intent::Complete directly; Voice/MCP requests only register a pending
//     confirmation that a physical confirm (Touch semantics) later releases,
//   - query kinds are NOT forwarded anywhere (they never mutate domain state;
//     the MCP host / UI presenters read them directly).
// The injected CommandSink performs the real transactional emit (host:
// AppCoordinator glue; device: P17 adapter). Building the domain
// ReducerContext (clock / id sources) is glue-side, never inside this layer.
#pragma once

#include <cstdint>
#include <optional>

#include "interaction/command.h"
#include "learning_domain/intents.h"

namespace claw4 {
namespace interaction {

// Which input channel raised the command (V4 §7: Touch / STT / MCP unified).
enum class CommandSource : uint8_t {
  Touch = 0,  // child physically on the device
  Voice,      // recognized STT phrase
  Mcp,        // learning.* MCP tool call
};

enum class DispatchStatus : uint8_t {
  Emitted = 0,            // mapped and forwarded to the sink
  NeedsUserConfirmation,  // CompleteTask from Voice/MCP: pending, NOT emitted
  NoPendingConfirmation,  // confirm requested while nothing is pending
  UnsupportedKind,        // kind cannot be dispatched as a domain mutation
};

struct DispatchResult {
  DispatchStatus status = DispatchStatus::UnsupportedKind;
  domain::IntentResult intent_result = domain::IntentResult::RejectedInvalidState;
};

// Where mapped intents are applied. Implementations own the transactional
// commit pipeline (coordinator) — the dispatcher never bypasses it.
class CommandSink {
 public:
  virtual ~CommandSink() = default;
  virtual domain::IntentResult emit(const domain::IntentRequest& intent) = 0;
};

class CommandDispatcher {
 public:
  explicit CommandDispatcher(CommandSink& sink) : sink_(sink) {}
  CommandDispatcher(const CommandDispatcher&) = delete;
  CommandDispatcher& operator=(const CommandDispatcher&) = delete;

  // Unified entry: routes a payload from any source. Query kinds and Unknown
  // return UnsupportedKind without touching the sink.
  DispatchResult dispatch(CommandSource source, const CommandPayload& payload);

  // Physical confirmation (Touch semantics) of a pending Voice/MCP completion
  // request. Emits Intent::Complete exactly once. AI can never complete a task
  // directly (V4 §8 / P15).
  DispatchResult confirmPendingComplete();

  void cancelPendingComplete();  // shell "dismiss" — no domain effect
  bool hasPendingComplete() const { return pending_complete_.has_value(); }

 private:
  static std::optional<domain::IntentRequest> mapMutation(const CommandPayload& payload);
  domain::IntentResult forward(const domain::IntentRequest& intent);

  CommandSink& sink_;
  std::optional<CommandPayload> pending_complete_;
};

}  // namespace interaction
}  // namespace claw4
