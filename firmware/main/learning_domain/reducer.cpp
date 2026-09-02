// claw4/firmware/main/learning_domain/reducer.cpp
// Pure deterministic domain reducer implementation.
// Portable C++17. No hardware/OS/sync dependencies.
#include "learning_domain/reducer.h"

#include <utility>

namespace claw4 {
namespace domain {

namespace {

// Accumulates the running focus segment into actual_seconds and returns the
// session with a cleared segment start. Rejects when the clock went backwards.
bool settleRunningSegment(StudySession& s, int64_t now_ms) {
  if (s.status != SessionStatus::Running || s.segment_start_monotonic_ms <= 0) {
    return true;  // nothing to settle
  }
  if (now_ms < s.segment_start_monotonic_ms) {
    return false;  // clock went backwards
  }
  s.actual_seconds += (now_ms - s.segment_start_monotonic_ms) / 1000;
  s.segment_start_monotonic_ms = 0;
  return true;
}

EventDraft makeDraft(const ReducerContext& ctx, EventType type,
                     std::map<std::string, std::string> payload = {}) {
  EventDraft d;
  d.event_id = ctx.make_event_id ? ctx.make_event_id() : EventId{};
  d.device_id = ctx.device_id;
  d.child_id = ctx.child_id;
  d.timestamp = ctx.now_epoch;
  d.timestamp_source = TimestampSource::Local;
  d.type = type;
  d.version = 1;
  d.payload = std::move(payload);
  return d;
}

int findTask(const DomainState& s, const TaskId& id) {
  for (std::size_t i = 0; i < s.tasks.size(); ++i) {
    if (s.tasks[i].task_id == id) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

TransitionResult DomainReducer::reduce(const DomainState& state,
                                       const IntentRequest& request,
                                       const ReducerContext& ctx) const {
  if (!request.task_id) {
    return {false, RejectReason::TaskNotFound, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  const int ti = findTask(state, *request.task_id);
  if (ti < 0) {
    return {false, RejectReason::TaskNotFound, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  const Task& task = state.tasks[static_cast<std::size_t>(ti)];
  DomainState next = state;
  std::vector<EventDraft> drafts;

  switch (request.intent) {
    case Intent::StartTask: {
      // Only a Ready (or previously interrupted/Paused) task may start.
      if (task.status == TaskStatus::Completed) {
        return {false, RejectReason::TaskAlreadyCompleted,
                IntentResult::Idempotent, std::nullopt, {}};
      }
      if (task.status == TaskStatus::Skipped) {
        return {false, RejectReason::TaskAlreadySkipped,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      if (task.status == TaskStatus::InProgress || next.active_session.has_value()) {
        return {false, RejectReason::TaskAlreadyStarted,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      if (!ctx.make_event_id || !ctx.make_session_id) {
        return {false, RejectReason::MissingIdSource,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      StudySession session;
      session.session_id = ctx.make_session_id();
      session.task_id = task.task_id;
      session.child_id = ctx.child_id;
      session.device_id = ctx.device_id;
      session.planned_minutes = task.estimated_minutes;
      session.status = SessionStatus::Running;
      session.segment_start_monotonic_ms = ctx.monotonic_ms;

      next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::InProgress;
      next.active_session = session;
      drafts.push_back(makeDraft(ctx, EventType::TaskStarted,
                                 {{"task_id", task.task_id.value}}));
      drafts.push_back(makeDraft(ctx, EventType::StudySessionStarted,
                                 {{"session_id", session.session_id.value},
                                  {"task_id", task.task_id.value}}));
      return {true, RejectReason::None, IntentResult::Accepted, next, std::move(drafts)};
    }

    case Intent::Pause: {
      if (task.status != TaskStatus::InProgress || !next.active_session) {
        return {false, RejectReason::TaskNotInProgress,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      StudySession& s = *next.active_session;
      if (s.status != SessionStatus::Running) {
        return {false, RejectReason::SessionNotActive,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      if (!settleRunningSegment(s, ctx.monotonic_ms)) {
        return {false, RejectReason::ClockWentBackwards,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      s.status = SessionStatus::Paused;
      s.paused_at_monotonic_ms = ctx.monotonic_ms;
      s.pause_count += 1;
      next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::Paused;
      drafts.push_back(makeDraft(ctx, EventType::TaskPaused,
                                 {{"task_id", task.task_id.value}}));
      return {true, RejectReason::None, IntentResult::Accepted, next, std::move(drafts)};
    }

    case Intent::Resume: {
      if (task.status != TaskStatus::Paused || !next.active_session) {
        return {false, RejectReason::TaskNotPaused,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      StudySession& s = *next.active_session;
      if (s.status != SessionStatus::Paused) {
        return {false, RejectReason::SessionNotPaused,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      if (s.paused_at_monotonic_ms > 0) {
        if (ctx.monotonic_ms < s.paused_at_monotonic_ms) {
          return {false, RejectReason::ClockWentBackwards,
                  IntentResult::RejectedInvalidState, std::nullopt, {}};
        }
        s.pause_seconds += (ctx.monotonic_ms - s.paused_at_monotonic_ms) / 1000;
        s.paused_at_monotonic_ms = 0;
      }
      s.status = SessionStatus::Running;
      s.segment_start_monotonic_ms = ctx.monotonic_ms;
      next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::InProgress;
      drafts.push_back(makeDraft(ctx, EventType::TaskResumed,
                                 {{"task_id", task.task_id.value}}));
      return {true, RejectReason::None, IntentResult::Accepted, next, std::move(drafts)};
    }

    case Intent::Complete: {
      if (task.status == TaskStatus::Completed) {
        return {false, RejectReason::TaskAlreadyCompleted,
                IntentResult::Idempotent, std::nullopt, {}};
      }
      if (task.status != TaskStatus::InProgress && task.status != TaskStatus::Paused) {
        return {false, RejectReason::TaskNotInProgress,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      if (!next.active_session) {
        return {false, RejectReason::SessionNotActive,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      StudySession done = *next.active_session;  // copy before clearing
      if (!settleRunningSegment(done, ctx.monotonic_ms)) {
        return {false, RejectReason::ClockWentBackwards,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      done.status = SessionStatus::Completed;
      done.completion_type = CompletionType::Manual;  // explicit user completion (I2)
      done.paused_at_monotonic_ms = 0;
      next.active_session = std::nullopt;
      next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::Completed;
      drafts.push_back(makeDraft(ctx, EventType::TaskCompleted,
                                 {{"task_id", task.task_id.value}}));
      drafts.push_back(makeDraft(
          ctx, EventType::StudySessionCompleted,
          {{"session_id", done.session_id.value}, {"task_id", task.task_id.value},
           {"completion_type", "manual"},
           {"actual_seconds", std::to_string(done.actual_seconds)}}));
      return {true, RejectReason::None, IntentResult::Accepted, next, std::move(drafts)};
    }

    case Intent::Skip: {
      // Skip is only allowed before the task has begun (Ready).
      if (task.status == TaskStatus::Skipped) {
        return {false, RejectReason::TaskAlreadySkipped,
                IntentResult::Idempotent, std::nullopt, {}};
      }
      if (task.status != TaskStatus::Ready) {
        return {false, RejectReason::SkipNotAllowed,
                IntentResult::RejectedInvalidState, std::nullopt, {}};
      }
      next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::Skipped;
      drafts.push_back(makeDraft(ctx, EventType::TaskSkipped,
                                 {{"task_id", task.task_id.value}}));
      return {true, RejectReason::None, IntentResult::Accepted, next, std::move(drafts)};
    }

    default:
      return {false, RejectReason::TaskNotFound, IntentResult::RejectedInvalidState,
              std::nullopt, {}};
  }
}

TransitionResult DomainReducer::onSegmentTimeout(const DomainState& state,
                                                 const SessionId& session_id,
                                                 const ReducerContext& ctx) const {
  if (!state.active_session || state.active_session->session_id != session_id) {
    return {false, RejectReason::SessionNotActive, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  const int ti = findTask(state, state.active_session->task_id);
  if (ti < 0 || state.tasks[static_cast<std::size_t>(ti)].status != TaskStatus::InProgress) {
    return {false, RejectReason::TaskNotInProgress, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  DomainState next = state;
  StudySession& s = *next.active_session;
  if (s.status != SessionStatus::Running) {
    return {false, RejectReason::SessionNotActive, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  if (!settleRunningSegment(s, ctx.monotonic_ms)) {
    return {false, RejectReason::ClockWentBackwards, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  // Timer expiry only pauses the focus segment for a user decision; it never
  // completes the session or the task (I2). No business event is emitted.
  s.status = SessionStatus::Paused;
  s.paused_at_monotonic_ms = ctx.monotonic_ms;
  s.pause_count += 1;
  next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::Paused;
  return {true, RejectReason::None, IntentResult::Accepted, next, {}};
}

TransitionResult DomainReducer::recoverSession(const DomainState& state,
                                               const SessionId& session_id,
                                               RecoveryMode mode,
                                               const ReducerContext& ctx) const {
  if (!state.active_session || state.active_session->session_id != session_id) {
    return {false, RejectReason::SessionNotActive, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  if (mode == RecoveryMode::Intact) {
    // Resume the SAME session; no state change and no drafts. The caller shows
    // the decision UI (continue vs save/end). Nothing needs committing.
    return {true, RejectReason::None, IntentResult::Accepted, state, {}};
  }
  // Corrupt snapshot: the session can only become Aborted; the Task must NOT
  // be auto-completed (CR-WB002-09). The task returns to Paused so it may be
  // started again later.
  DomainState next = state;
  StudySession aborted = *next.active_session;  // copy before clearing
  const int ti = findTask(next, next.active_session->task_id);
  aborted.status = SessionStatus::Aborted;
  aborted.completion_type = CompletionType::Aborted;
  aborted.segment_start_monotonic_ms = 0;
  aborted.paused_at_monotonic_ms = 0;
  if (ti >= 0 && next.tasks[static_cast<std::size_t>(ti)].status == TaskStatus::InProgress) {
    next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::Paused;
  }
  next.active_session = std::nullopt;
  std::vector<EventDraft> drafts;
  drafts.push_back(makeDraft(
      ctx, EventType::StudySessionCompleted,
      {{"session_id", aborted.session_id.value},
       {"completion_type", "aborted"},
       {"actual_seconds", std::to_string(aborted.actual_seconds)}}));
  return {true, RejectReason::None, IntentResult::Accepted, next, std::move(drafts)};
}

TransitionResult DomainReducer::endRecoveredSession(const DomainState& state,
                                                    const SessionId& session_id,
                                                    const ReducerContext& ctx) const {
  if (!state.active_session || state.active_session->session_id != session_id) {
    return {false, RejectReason::SessionNotActive, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  DomainState next = state;
  const int ti = findTask(next, next.active_session->task_id);
  StudySession saved = *next.active_session;  // copy before clearing
  if (!settleRunningSegment(saved, ctx.monotonic_ms)) {
    return {false, RejectReason::ClockWentBackwards, IntentResult::RejectedInvalidState,
            std::nullopt, {}};
  }
  saved.status = SessionStatus::Completed;
  saved.completion_type = CompletionType::AutoSaved;  // user explicitly chose save/end
  saved.paused_at_monotonic_ms = 0;
  if (ti >= 0) {
    // Task is NOT auto-completed; it returns to Paused for a later start.
    next.tasks[static_cast<std::size_t>(ti)].status = TaskStatus::Paused;
  }
  next.active_session = std::nullopt;
  std::vector<EventDraft> drafts;
  drafts.push_back(makeDraft(
      ctx, EventType::StudySessionCompleted,
      {{"session_id", saved.session_id.value},
       {"completion_type", "auto_saved"},
       {"actual_seconds", std::to_string(saved.actual_seconds)}}));
  return {true, RejectReason::None, IntentResult::Accepted, next, std::move(drafts)};
}

}  // namespace domain
}  // namespace claw4
