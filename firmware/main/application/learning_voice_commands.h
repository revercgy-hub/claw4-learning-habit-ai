#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "application/coordinator.h"
#include "reminder/reminder_core.h"
#include "ui/presenters.h"

namespace claw4::application {

enum class LearningVoiceKind {
  QueryTodayTasks, StartTask, PauseTask, ResumeTask, CompleteRequest,
  QueryRemainingTime, SnoozeReminder,
};

enum class LearningVoiceStatus {
  Applied, ConfirmationRequired, AmbiguousTask, InvalidState, Stale,
  Busy, StorageError,
};

struct LearningVoiceRequest {
  std::string command_id;
  int64_t session_epoch;
  LearningVoiceKind kind;
  std::optional<domain::TaskId> task_id;
  std::optional<int64_t> expected_task_version;
  int64_t expires_at_ms;
};

// The runtime supplies this from the authenticated session, never from the
// request body. An absent binding cannot execute any command.
struct LearningVoiceBinding {
  domain::DeviceId device_id;
  domain::ChildId child_id;
  int64_t session_epoch;
};

struct LearningVoiceReply {
  LearningVoiceStatus status;
  std::vector<domain::Task> tasks{};
  std::optional<int64_t> snapshot_epoch_ms{};
  std::optional<ui::FocusView> focus{};
  std::optional<int64_t> task_version{};
};

// Call from the learning runtime's serialized transaction context. The runtime
// owns snapshot provenance, the current reminder, and authenticated binding.
class LearningVoiceCommands {
 public:
  LearningVoiceCommands(AppCoordinator& coordinator, reminder::ReminderCore& reminders)
      : coordinator_(coordinator), reminders_(reminders) {}

  LearningVoiceReply execute(const LearningVoiceRequest& request,
                             const std::optional<LearningVoiceBinding>& binding,
                             const domain::ReducerContext& context,
                             std::optional<int64_t> trusted_epoch_ms,
                             std::optional<int64_t> snapshot_epoch_ms,
                             const std::optional<std::string>& presented_reminder);

  // Only the physical touch adapter calls this entry point. Voice has no
  // completion action that can reach it.
  LearningVoiceReply confirmFromTouch(const std::optional<LearningVoiceBinding>& binding,
                                     const domain::ReducerContext& context,
                                     std::optional<int64_t> trusted_epoch_ms);
  void cancelConfirmation() { pending_.reset(); }

 private:
  struct Pending {
    domain::DeviceId device_id;
    domain::ChildId child_id;
    domain::TaskId task_id;
    domain::SessionId session_id;
    int64_t session_epoch;
    int64_t task_version;
    domain::SessionStatus session_status;
    int pause_count;
    int64_t expires_at_ms;
  };
  AppCoordinator& coordinator_;
  reminder::ReminderCore& reminders_;
  std::optional<Pending> pending_;
};

}  // namespace claw4::application
