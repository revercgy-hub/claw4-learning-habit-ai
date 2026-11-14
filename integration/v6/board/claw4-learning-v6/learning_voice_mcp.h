#pragma once

#ifdef CLAW4_LEARNING_PREVIEW
#include <functional>

#include "application/learning_voice_exchange.h"

namespace claw4::v6 {

struct LearningVoiceCallContext {
  application::LearningVoiceBinding binding;
  domain::ReducerContext reducer;
  std::optional<int64_t> now;
  std::optional<int64_t> snapshot;
  std::optional<std::string> presented_reminder;
};

// Called once by the paired Learning Runtime. Its callback runs on the
// Application task and returns no context when the current lease is invalid.
void RegisterLearningVoiceTool(
    application::LearningVoiceCommands& commands, application::LearningVoiceMac signer,
    std::function<std::optional<LearningVoiceCallContext>()> current);

application::LearningVoiceMac CreateLearningVoiceSigner(std::string key);

}  // namespace claw4::v6
#endif
