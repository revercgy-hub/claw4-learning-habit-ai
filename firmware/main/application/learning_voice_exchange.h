#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "application/learning_voice_commands.h"

namespace claw4::application {

// Returns a Base64 HMAC-SHA256 for the complete message, or an empty string.
using LearningVoiceMac = std::function<std::string(std::string_view)>;

struct LearningVoiceEnvelope {
  std::string command_id;
  std::string kind;
  int64_t session_epoch;
  int64_t expires_at_ms;
};

struct LearningVoiceResult {
  std::string status;
  std::optional<int64_t> value;
  std::optional<int64_t> snapshot_epoch_ms;
  std::string mac;
};

// The caller owns the current paired identity/session, trusted time and MAC
// key. Invoke in the same serialized context as touch and the coordinator.
std::optional<LearningVoiceResult> exchangeLearningVoice(
    LearningVoiceCommands& commands, const LearningVoiceEnvelope& envelope,
    std::string_view request_mac, const std::optional<LearningVoiceBinding>& binding,
    const LearningVoiceMac& sign, const domain::ReducerContext& context,
    std::optional<int64_t> now, std::optional<int64_t> snapshot,
    const std::optional<std::string>& presented_reminder);

}  // namespace claw4::application
