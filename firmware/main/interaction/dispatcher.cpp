// claw4/firmware/main/interaction/dispatcher.cpp
// Implementation of the single interaction funnel (WB-LEARNING-V4 P14).
#include "interaction/dispatcher.h"

#include <utility>

namespace claw4 {
namespace interaction {

namespace {

// Pure kind -> domain intent mapping. Query kinds and Unknown are not
// mutations and map to nullopt (they never enter the reducer).
std::optional<domain::Intent> toIntent(CommandKind kind) {
  switch (kind) {
    case CommandKind::StartTask:
      return domain::Intent::StartTask;
    case CommandKind::PauseTask:
      return domain::Intent::Pause;
    case CommandKind::ResumeTask:
      return domain::Intent::Resume;
    case CommandKind::CompleteTask:
      return domain::Intent::Complete;
    case CommandKind::SkipTask:
      return domain::Intent::Skip;
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<domain::IntentRequest> CommandDispatcher::mapMutation(
    const CommandPayload& payload) {
  const auto intent = toIntent(payload.kind);
  if (!intent) return std::nullopt;
  domain::IntentRequest request;
  request.intent = *intent;
  request.task_id = payload.task_id;
  request.session_id = payload.session_id;
  return request;
}

domain::IntentResult CommandDispatcher::forward(const domain::IntentRequest& intent) {
  return sink_.emit(intent);
}

DispatchResult CommandDispatcher::dispatch(CommandSource source,
                                           const CommandPayload& payload) {
  if (payload.kind == CommandKind::CompleteTask) {
    if (source != CommandSource::Touch) {
      // Voice/MCP completion request: register for physical confirmation only.
      // Re-requesting overwrites the pending payload but never emits twice.
      pending_complete_ = payload;
      return {DispatchStatus::NeedsUserConfirmation,
              domain::IntentResult::RejectedInvalidState};
    }
    // A physical Touch completion is itself the confirmation: any pending
    // AI/voice request is consumed by this explicit user action.
    pending_complete_.reset();
  }

  const auto request = mapMutation(payload);
  if (!request) {
    return {DispatchStatus::UnsupportedKind,
            domain::IntentResult::RejectedInvalidState};
  }
  return {DispatchStatus::Emitted, forward(*request)};
}

DispatchResult CommandDispatcher::confirmPendingComplete() {
  if (!pending_complete_) {
    return {DispatchStatus::NoPendingConfirmation,
            domain::IntentResult::RejectedInvalidState};
  }
  const CommandPayload payload = *pending_complete_;
  pending_complete_.reset();
  const auto request = mapMutation(payload);  // kind is CompleteTask here
  if (!request) {
    return {DispatchStatus::UnsupportedKind,
            domain::IntentResult::RejectedInvalidState};
  }
  return {DispatchStatus::Emitted, forward(*request)};
}

void CommandDispatcher::cancelPendingComplete() { pending_complete_.reset(); }

}  // namespace interaction
}  // namespace claw4
