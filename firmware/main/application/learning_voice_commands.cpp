#include "application/learning_voice_commands.h"

#include <algorithm>

namespace claw4::application {
namespace {

using Status = LearningVoiceStatus;
using Kind = LearningVoiceKind;

bool bound(const std::optional<LearningVoiceBinding>& binding,
           const domain::ReducerContext& context) {
  return binding && binding->session_epoch > 0 && !binding->device_id.empty() &&
         !binding->child_id.empty() && binding->device_id == context.device_id &&
         binding->child_id == context.child_id;
}

const domain::Task* findTask(const domain::DomainState& state, const domain::ChildId& child,
                            const domain::TaskId& id) {
  const auto it = std::find_if(state.tasks.begin(), state.tasks.end(), [&](const auto& task) {
    return task.task_id == id && task.child_id == child;
  });
  return it == state.tasks.end() ? nullptr : &*it;
}

bool hasActiveSession(const domain::DomainState& state, const domain::ReducerContext& context) {
  return state.active_session && state.active_session->device_id == context.device_id &&
         state.active_session->child_id == context.child_id &&
         (state.active_session->status == domain::SessionStatus::Running ||
          state.active_session->status == domain::SessionStatus::Paused);
}

std::optional<domain::Intent> taskMutation(Kind kind) {
  switch (kind) {
    case Kind::StartTask: return domain::Intent::StartTask;
    case Kind::PauseTask: return domain::Intent::Pause;
    case Kind::ResumeTask: return domain::Intent::Resume;
    default: return std::nullopt;
  }
}

LearningVoiceReply apply(AppCoordinator& coordinator, domain::Intent intent,
                         domain::TaskId task, std::optional<domain::SessionId> session,
                         const domain::ReducerContext& context) {
  const auto result = coordinator.dispatchIntent({intent, task, session}, context);
  if (result.intent_result == domain::IntentResult::PersistFailed) return {Status::StorageError};
  if (!result.ok) return {Status::InvalidState};
  LearningVoiceReply reply{Status::Applied};
  if (const auto* updated = findTask(coordinator.state(), context.child_id, task)) {
    reply.task_version = updated->version;
  }
  return reply;
}

}  // namespace

LearningVoiceReply LearningVoiceCommands::execute(
    const LearningVoiceRequest& request, const std::optional<LearningVoiceBinding>& binding,
    const domain::ReducerContext& context, std::optional<int64_t> now,
    std::optional<int64_t> snapshot_epoch_ms, const std::optional<std::string>& presented_reminder) {
  if (!bound(binding, context) || request.session_epoch != binding->session_epoch ||
      !now || *now <= 0 || request.expires_at_ms <= *now) return {Status::Stale};
  if (request.expires_at_ms - *now > domain::LearningCommandReceipt::kMaxLifetimeMs ||
      request.command_id.empty() || request.command_id.size() > 64 ||
      (request.task_id && (request.task_id->empty() || request.task_id->value.size() > 128)))
    return {Status::InvalidState};

  const auto& state = coordinator_.state();
  if (request.expires_at_ms <= state.learning_command_expired_through_ms)
    return {Status::Stale};
  const auto mutation = taskMutation(request.kind);
  std::size_t live_receipts = 0;
  for (const auto& row : state.learning_command_receipts) {
    if (row.expires_at_ms > *now) ++live_receipts;
    if (row.device_id != binding->device_id || row.child_id != binding->child_id ||
        row.session_epoch != binding->session_epoch || row.command_id != request.command_id) continue;
    if (row.intent != mutation || row.requested_task != request.task_id ||
        row.expected_task_version != request.expected_task_version ||
        row.expires_at_ms != request.expires_at_ms) return {Status::Stale};
    return {Status::Applied, {}, {}, {}, row.applied_task_version};
  }
  const bool active = hasActiveSession(state, context);
  if (request.kind == Kind::QueryTodayTasks) {
    if (!snapshot_epoch_ms || *snapshot_epoch_ms <= 0 || *snapshot_epoch_ms > *now)
      return {Status::Stale};
    LearningVoiceReply reply{Status::Applied};
    reply.snapshot_epoch_ms = snapshot_epoch_ms;
    for (const auto& task : state.tasks) {
      if (task.child_id == context.child_id) reply.tasks.push_back(task);
    }
    return reply;
  }
  if (request.kind == Kind::QueryRemainingTime) {
    if (!active) return {Status::InvalidState};
    ui::ViewState view;
    view.domain = state;
    LearningVoiceReply reply{Status::Applied};
    reply.focus = ui::buildFocus(view, context.monotonic_ms);
    return reply;
  }
  if (request.kind == Kind::SnoozeReminder) {
    if (!presented_reminder) return {Status::InvalidState};
    const auto& records = reminders_.records();
    const auto record = std::find_if(records.begin(), records.end(), [&](const auto& item) {
      return item.instance_id == *presented_reminder && item.child_id == context.child_id &&
             item.active && !item.delivered_epoch_ms && item.kind != reminder::ReminderKind::ManualAlarm;
    });
    if (record == records.end()) return {Status::InvalidState};
    if (record->snooze_until_epoch_ms > *now) return {Status::Applied};
    return {reminders_.snooze(record->instance_id, *now) ? Status::Applied : Status::StorageError};
  }

  const domain::Task* task = nullptr;
  if (request.kind == Kind::StartTask) {
    if (state.active_session) return {Status::Busy};
    if (request.task_id) {
      task = findTask(state, context.child_id, *request.task_id);
    } else {
      for (const auto& candidate : state.tasks) {
        if (candidate.child_id != context.child_id || candidate.status != domain::TaskStatus::Ready) continue;
        if (task) return {Status::AmbiguousTask};
        task = &candidate;
      }
    }
  } else if (active && (!request.task_id || *request.task_id == state.active_session->task_id)) {
    task = findTask(state, context.child_id, state.active_session->task_id);
  }
  if (!task) return {Status::InvalidState};
  if (request.expected_task_version && *request.expected_task_version != task->version)
    return {Status::Stale};

  const auto session = active ? std::optional<domain::SessionId>(state.active_session->session_id)
                              : std::nullopt;
  if (mutation) {
    if (live_receipts >= domain::LearningCommandReceipt::kCapacity) return {Status::Busy};
    const int64_t version = task->version;
    const auto result = coordinator_.dispatchLearningCommand(
        {*mutation, task->task_id, session}, context,
        {binding->device_id, binding->child_id, binding->session_epoch, request.command_id,
         *mutation, request.task_id, request.expected_task_version, request.expires_at_ms, version},
        *now);
    if (result.intent_result == domain::IntentResult::PersistFailed) return {Status::StorageError};
    if (!result.ok) return {Status::InvalidState};
    return {Status::Applied, {}, {}, {}, version};
  }
  if (request.kind != Kind::CompleteRequest || !session) return {Status::InvalidState};
  pending_ = Pending{context.device_id, context.child_id, task->task_id, *session,
                     binding->session_epoch, task->version, state.active_session->status,
                     state.active_session->pause_count, request.expires_at_ms};
  return {Status::ConfirmationRequired, {}, {}, {}, task->version};
}

LearningVoiceReply LearningVoiceCommands::confirmFromTouch(
    const std::optional<LearningVoiceBinding>& binding, const domain::ReducerContext& context,
    std::optional<int64_t> now) {
  if (!pending_) return {Status::InvalidState};
  const auto& pending = *pending_;
  const auto& state = coordinator_.state();
  const auto* task = findTask(state, context.child_id, pending.task_id);
  if (!bound(binding, context) || pending.session_epoch != binding->session_epoch ||
      pending.device_id != context.device_id || pending.child_id != context.child_id ||
      !now || *now <= 0 || *now >= pending.expires_at_ms || !task ||
      task->version != pending.task_version || !hasActiveSession(state, context) ||
      state.active_session->task_id != pending.task_id ||
      state.active_session->session_id != pending.session_id ||
      state.active_session->status != pending.session_status ||
      state.active_session->pause_count != pending.pause_count) {
    pending_.reset();
    return {Status::Stale};
  }
  auto reply = apply(coordinator_, domain::Intent::Complete, pending.task_id, pending.session_id, context);
  if (reply.status != Status::StorageError) pending_.reset();
  return reply;
}

}  // namespace claw4::application
