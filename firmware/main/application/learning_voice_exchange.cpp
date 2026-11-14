#include "application/learning_voice_exchange.h"

#include <algorithm>
#include <array>

namespace claw4::application {
namespace {

using Kind = LearningVoiceKind;
using Status = LearningVoiceStatus;

constexpr std::array kKinds{
    std::pair{"QUERY_TODAY_TASKS", Kind::QueryTodayTasks},
    std::pair{"START_TASK", Kind::StartTask},
    std::pair{"PAUSE_TASK", Kind::PauseTask},
    std::pair{"RESUME_TASK", Kind::ResumeTask},
    std::pair{"COMPLETE_REQUEST", Kind::CompleteRequest},
    std::pair{"QUERY_REMAINING_TIME", Kind::QueryRemainingTime},
    std::pair{"SNOOZE_REMINDER", Kind::SnoozeReminder},
};

const char* statusName(Status status) {
  switch (status) {
    case Status::Applied: return "APPLIED";
    case Status::ConfirmationRequired: return "CONFIRMATION_REQUIRED";
    case Status::AmbiguousTask: return "AMBIGUOUS_TASK";
    case Status::InvalidState: return "INVALID_STATE";
    case Status::Stale: return "STALE";
    case Status::Busy: return "BUSY";
    case Status::StorageError: return "STORAGE_ERROR";
  }
  return "INVALID_STATE";
}

bool validField(std::string_view field, std::size_t limit) {
  return !field.empty() && field.size() <= limit &&
      std::all_of(field.begin(), field.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == ':' || c == '.';
      });
}

bool sameMac(std::string_view expected, std::string_view received) {
  if (expected.size() != 44 || received.size() != expected.size()) return false;
  unsigned different = 0;
  for (std::size_t i = 0; i < expected.size(); ++i) different |= expected[i] ^ received[i];
  return different == 0;
}

std::string macNumber(std::optional<int64_t> value) {
  return value ? std::to_string(*value) : "-";
}

}  // namespace

std::string signLearningVoiceSession(const LearningVoiceBinding& binding,
                                    std::string_view connection_id, std::string_view challenge,
                                    const LearningVoiceMac& sign) {
  if (!sign || binding.session_epoch <= 0 ||
      !validField(binding.device_id.value, 128) || !validField(binding.child_id.value, 128) ||
      connection_id.size() != 36 || !validField(connection_id, 36) || challenge.size() != 32 ||
      !std::all_of(challenge.begin(), challenge.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      })) return {};
  return sign("claw4-learning-session-v1\n" + binding.device_id.value + "\n" + binding.child_id.value +
              "\n" + std::string(connection_id) + "\n" + std::string(challenge) + "\n" +
              std::to_string(binding.session_epoch));
}

std::optional<LearningVoiceResult> exchangeLearningVoice(
    LearningVoiceCommands& commands, const LearningVoiceEnvelope& envelope,
    std::string_view request_mac, const std::optional<LearningVoiceBinding>& binding,
    const LearningVoiceMac& sign, const domain::ReducerContext& context,
    std::optional<int64_t> now, std::optional<int64_t> snapshot,
    const std::optional<std::string>& presented_reminder) {
  if (!binding || !sign || !now || *now <= 0 || binding->session_epoch <= 0 ||
      envelope.session_epoch != binding->session_epoch || envelope.expires_at_ms <= *now ||
      envelope.expires_at_ms - *now > domain::LearningCommandReceipt::kMaxLifetimeMs ||
      !validField(binding->device_id.value, 128) || !validField(binding->child_id.value, 128) ||
      !validField(envelope.command_id, 64)) return std::nullopt;
  const auto kind = std::find_if(kKinds.begin(), kKinds.end(), [&](const auto& item) {
    return envelope.kind == item.first;
  });
  if (kind == kKinds.end()) return std::nullopt;

  const std::string fields = binding->device_id.value + "\n" + binding->child_id.value + "\n" +
      std::to_string(envelope.session_epoch) + "\n" + envelope.command_id + "\n" + envelope.kind +
      "\n" + std::to_string(envelope.expires_at_ms);
  if (!sameMac(sign("claw4-learning-request-v1\n" + fields), request_mac)) return std::nullopt;

  const auto reply = commands.execute({envelope.command_id, envelope.session_epoch, kind->second,
                                       {}, {}, envelope.expires_at_ms},
                                      binding, context, now, snapshot, presented_reminder);
  LearningVoiceResult result{statusName(reply.status), {}, {}, {}};
  if (reply.status == Status::Applied && kind->second == Kind::QueryTodayTasks) {
    result.value = int64_t(reply.tasks.size());
    result.snapshot_epoch_ms = reply.snapshot_epoch_ms;
  } else if (reply.status == Status::Applied && kind->second == Kind::QueryRemainingTime && reply.focus) {
    const auto ms = reply.focus->remaining_ms;
    result.value = ms / 1000 + (ms % 1000 != 0);
  }
  result.mac = sign("claw4-learning-result-v1\n" + fields + "\n" + result.status + "\n" +
                    macNumber(result.value) + "\n" + macNumber(result.snapshot_epoch_ms));
  if (result.mac.size() != 44) return std::nullopt;
  return result;
}

}  // namespace claw4::application
