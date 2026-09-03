// claw4/firmware/tests/unit/domain/domain_reducer_tests.cpp
// Host unit tests for the pure domain reducer (WB-STREAM-002 CP1).
//
// Compiles, links and RUNS natively on Windows. Uses an explicit case/assert
// counter (NOT bare `assert`, which vanishes under NDEBUG) and returns non-zero
// on failure. Deterministic: all ids/times are injected through ReducerContext.

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/reducer.h"

using namespace claw4::domain;

// ---------------------------------------------------------------------------
// tiny test harness
// ---------------------------------------------------------------------------
static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                                   \
  do {                                               \
    ++g_cases;                                       \
    if (!run_case_##name()) {                        \
      std::printf("FAIL: %s\n", #name);              \
      ++g_fail;                                      \
    }                                                \
  } while (0)

#define CHECK(cond)                                  \
  do {                                               \
    if (!(cond)) {                                   \
      std::printf("  assert fail: %s (line %d)\n",   \
                  #cond, __LINE__);                  \
      return false;                                  \
    }                                                \
  } while (0)

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
struct CtxBuilder {
  DeviceId device{"dev-1"};
  ChildId child{"child-1"};
  int64_t epoch = 1756718530;
  int64_t mono = 1'000'000;          // monotonic ms cursor
  int event_seq = 0;
  int session_seq = 0;

  ReducerContext make() {
    ReducerContext c;
    c.device_id = device;
    c.child_id = child;
    c.now_epoch = epoch;
    c.monotonic_ms = mono;
    c.make_event_id = [this]() {
      ++event_seq;
      return EventId{"ev-" + std::to_string(event_seq)};
    };
    c.make_session_id = [this]() {
      ++session_seq;
      return SessionId{"sess-" + std::to_string(session_seq)};
    };
    return c;
  }

  void advance_ms(int64_t ms) { mono += ms; }
};

DomainState makeState(TaskStatus st = TaskStatus::Ready) {
  DomainState s;
  Task t;
  t.task_id = TaskId{"task-1"};
  t.child_id = ChildId{"child-1"};
  t.title = "练习册 P32";
  t.estimated_minutes = 25;
  t.status = st;
  s.tasks.push_back(t);
  s.device_state = DeviceState::OnlineIdle;
  s.time_synced = true;
  return s;
}

IntentRequest intentOf(Intent it, const std::string& task_id = "task-1") {
  IntentRequest r;
  r.intent = it;
  r.task_id = TaskId{task_id};
  return r;
}

// Validates that a failure produced no next state and no drafts.
bool failurePreservesState(const TransitionResult& res) {
  if (res.ok) return false;
  if (!res.drafts.empty()) return false;
  if (res.next.has_value()) return false;
  return res.intent_result != IntentResult::Accepted;
}

// ---------------------------------------------------------------------------
// case implementations
// ---------------------------------------------------------------------------
static bool run_case_start_ready_to_in_progress_two_drafts() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(r.ok);
  CHECK(r.drafts.size() == 2);
  CHECK(r.drafts[0].type == EventType::TaskStarted);
  CHECK(r.drafts[1].type == EventType::StudySessionStarted);
  CHECK(r.next->tasks[0].status == TaskStatus::InProgress);
  CHECK(r.next->active_session.has_value());
  CHECK(r.next->active_session->status == SessionStatus::Running);
  CHECK(r.next->active_session->task_id == TaskId{"task-1"});
  CHECK(!r.next->active_session->completion_type.has_value());
  return true;
}

static bool run_case_start_event_order_task_before_session() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(r.ok);
  CHECK(r.drafts[0].event_id.value == "ev-1");
  CHECK(r.drafts[1].event_id.value == "ev-2");
  // task.started must precede study.session.started.
  CHECK(r.drafts[0].type == EventType::TaskStarted);
  CHECK(r.drafts[1].type == EventType::StudySessionStarted);
  CHECK(r.drafts[0].payload.at("task_id") == "task-1");
  CHECK(r.drafts[1].payload.at("session_id") == "sess-1");
  return true;
}

static bool run_case_start_on_completed_rejected_idempotent() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::Completed);
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(!r.ok);
  CHECK(r.intent_result == IntentResult::Idempotent);
  CHECK(r.drafts.empty());
  return failurePreservesState(r);
}

static bool run_case_start_on_in_progress_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::InProgress);
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::TaskAlreadyStarted);
  return failurePreservesState(r);
}

static bool run_case_start_with_active_session_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::InProgress);
  StudySession active;
  active.session_id = SessionId{"sess-x"};
  active.task_id = TaskId{"task-1"};
  active.status = SessionStatus::Running;
  s0.active_session = active;
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::TaskAlreadyStarted);
  return failurePreservesState(r);
}

static bool run_case_start_on_skipped_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::Skipped);
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::TaskAlreadySkipped);
  return failurePreservesState(r);
}

static bool run_case_start_on_pending_rejected_task_not_ready() {
  // A task planned for a future date (TaskStatus::Pending) is not runnable
  // today: StartTask must be rejected with TaskNotReady, no state change and
  // no drafts. The server only serves today's tasks as Ready.
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::Pending);
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::TaskNotReady);
  CHECK(r.intent_result == IntentResult::RejectedInvalidState);
  return failurePreservesState(r);
}

static bool run_case_start_missing_id_source_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState();
  ReducerContext c = b.make();
  c.make_event_id = {};  // no id source injected
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), c);
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::MissingIdSource);
  return failurePreservesState(r);
}

static bool run_case_pause_accumulates_and_marks_paused() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto r1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(r1.ok);
  DomainState s1 = *r1.next;
  b.advance_ms(100'000);  // 100 s of focus
  auto r2 = DomainReducer{}.reduce(s1, intentOf(Intent::Pause), b.make());
  CHECK(r2.ok);
  CHECK(r2.drafts.size() == 1 && r2.drafts[0].type == EventType::TaskPaused);
  CHECK(r2.next->tasks[0].status == TaskStatus::Paused);
  CHECK(r2.next->active_session->status == SessionStatus::Paused);
  CHECK(r2.next->active_session->pause_count == 1);
  CHECK(r2.next->active_session->actual_seconds == 100);
  return true;
}

static bool run_case_pause_requires_running() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::Paused);  // no active session
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::Pause), b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::TaskNotInProgress);
  return failurePreservesState(r);
}

static bool run_case_resume_requires_paused() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::InProgress);  // session Running
  StudySession active;
  active.session_id = SessionId{"sess-x"};
  active.task_id = TaskId{"task-1"};
  active.status = SessionStatus::Running;
  s0.active_session = active;
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::Resume), b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::TaskNotPaused);
  return failurePreservesState(r);
}

static bool run_case_resume_accumulates_pause_seconds() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(s1.ok);
  DomainState st = *s1.next;
  b.advance_ms(60'000);  // 60 s focus
  auto p = DomainReducer{}.reduce(st, intentOf(Intent::Pause), b.make());
  CHECK(p.ok);
  DomainState paused = *p.next;
  b.advance_ms(30'000);  // 30 s paused
  auto r = DomainReducer{}.reduce(paused, intentOf(Intent::Resume), b.make());
  CHECK(r.ok);
  CHECK(r.drafts.size() == 1 && r.drafts[0].type == EventType::TaskResumed);
  CHECK(r.next->tasks[0].status == TaskStatus::InProgress);
  CHECK(r.next->active_session->status == SessionStatus::Running);
  CHECK(r.next->active_session->actual_seconds == 60);
  CHECK(r.next->active_session->pause_seconds == 30);
  return true;
}

static bool run_case_multiple_pause_resume_rounds() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  // focus 60s, pause 10s, focus 90s, pause 5s
  b.advance_ms(60'000);
  auto p1 = DomainReducer{}.reduce(st, intentOf(Intent::Pause), b.make());
  DomainState a = *p1.next;
  b.advance_ms(10'000);
  auto r1 = DomainReducer{}.reduce(a, intentOf(Intent::Resume), b.make());
  DomainState f = *r1.next;
  b.advance_ms(90'000);
  auto p2 = DomainReducer{}.reduce(f, intentOf(Intent::Pause), b.make());
  DomainState b2 = *p2.next;
  b.advance_ms(5'000);
  auto r2 = DomainReducer{}.reduce(b2, intentOf(Intent::Resume), b.make());
  DomainState f2 = *r2.next;
  CHECK(r2.ok);
  CHECK(f2.active_session->pause_count == 2);
  CHECK(f2.active_session->actual_seconds == 150);   // 60+90
  CHECK(f2.active_session->pause_seconds == 15);     // 10+5
  return true;
}

static bool run_case_clock_backwards_on_pause_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  ReducerContext c = b.make();
  c.monotonic_ms = 100;  // behind segment start (1'000'000)
  auto r = DomainReducer{}.reduce(st, intentOf(Intent::Pause), c);
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::ClockWentBackwards);
  return failurePreservesState(r);
}

static bool run_case_clock_backwards_on_resume_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  b.advance_ms(50'000);
  auto p = DomainReducer{}.reduce(st, intentOf(Intent::Pause), b.make());
  DomainState paused = *p.next;
  ReducerContext c = b.make();
  c.monotonic_ms = 100;  // behind paused_at
  auto r = DomainReducer{}.reduce(paused, intentOf(Intent::Resume), c);
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::ClockWentBackwards);
  return failurePreservesState(r);
}

static bool run_case_segment_timeout_pauses_never_completes() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  b.advance_ms(25 * 60'000);  // planned 25 min reached
  auto r = DomainReducer{}.onSegmentTimeout(st, st.active_session->session_id, b.make());
  CHECK(r.ok);
  CHECK(r.next->tasks[0].status == TaskStatus::Paused);          // not Completed
  CHECK(r.next->active_session->status == SessionStatus::Paused);  // not Completed
  CHECK(!r.next->active_session->completion_type.has_value());
  CHECK(r.drafts.empty());  // no completion event, no auto-complete
  return true;
}

static bool run_case_after_timeout_user_completes() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  b.advance_ms(25 * 60'000);
  auto to = DomainReducer{}.onSegmentTimeout(st, st.active_session->session_id, b.make());
  CHECK(to.ok);
  DomainState paused = *to.next;
  auto r = DomainReducer{}.reduce(paused, intentOf(Intent::Complete), b.make());
  CHECK(r.ok);
  CHECK(r.next->tasks[0].status == TaskStatus::Completed);
  CHECK(!r.next->active_session.has_value());
  CHECK(r.drafts.size() == 2);
  CHECK(r.drafts[1].type == EventType::StudySessionCompleted);
  CHECK(r.drafts[1].payload.at("completion_type") == "manual");
  return true;
}

static bool run_case_complete_from_running_manual() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  b.advance_ms(1500'000);  // 25 min
  auto r = DomainReducer{}.reduce(st, intentOf(Intent::Complete), b.make());
  CHECK(r.ok);
  CHECK(r.drafts.size() == 2);
  CHECK(r.drafts[0].type == EventType::TaskCompleted);
  CHECK(r.drafts[1].type == EventType::StudySessionCompleted);
  CHECK(r.drafts[1].payload.at("completion_type") == "manual");
  // FIX-10 invariant: the manual completion draft carries its task_id.
  CHECK(r.drafts[1].payload.at("task_id") == "task-1");
  CHECK(r.next->tasks[0].status == TaskStatus::Completed);
  CHECK(r.next->active_session.has_value() == false);
  return true;
}

static bool run_case_complete_from_paused_allowed() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  b.advance_ms(60'000);
  auto p = DomainReducer{}.reduce(st, intentOf(Intent::Pause), b.make());
  DomainState paused = *p.next;
  auto r = DomainReducer{}.reduce(paused, intentOf(Intent::Complete), b.make());
  CHECK(r.ok);
  CHECK(r.next->tasks[0].status == TaskStatus::Completed);
  return true;
}

static bool run_case_duplicate_complete_idempotent() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState st = *s1.next;
  b.advance_ms(60'000);
  auto c1 = DomainReducer{}.reduce(st, intentOf(Intent::Complete), b.make());
  CHECK(c1.ok);
  DomainState done = *c1.next;
  auto c2 = DomainReducer{}.reduce(done, intentOf(Intent::Complete), b.make());
  CHECK(!c2.ok);
  CHECK(c2.intent_result == IntentResult::Idempotent);
  CHECK(c2.drafts.empty());  // duplicate Complete emits nothing (task.completed
                             // already generated once)
  return failurePreservesState(c2);
}

static bool run_case_complete_without_active_session_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::InProgress);  // no session
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::Complete), b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::SessionNotActive);
  return failurePreservesState(r);
}

static bool run_case_skip_only_when_ready() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::Ready);
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::Skip), b.make());
  CHECK(r.ok);
  CHECK(r.drafts.size() == 1 && r.drafts[0].type == EventType::TaskSkipped);
  CHECK(r.next->tasks[0].status == TaskStatus::Skipped);
  // Skip is not allowed once the task has begun.
  DomainState ip = makeState(TaskStatus::InProgress);
  auto r2 = DomainReducer{}.reduce(ip, intentOf(Intent::Skip), b.make());
  CHECK(!r2.ok);
  CHECK(r2.reason == RejectReason::SkipNotAllowed);
  return failurePreservesState(r2);
}

static bool run_case_duplicate_skip_idempotent() {
  CtxBuilder b;
  DomainState s0 = makeState(TaskStatus::Skipped);
  auto r = DomainReducer{}.reduce(s0, intentOf(Intent::Skip), b.make());
  CHECK(!r.ok);
  CHECK(r.intent_result == IntentResult::Idempotent);
  CHECK(r.drafts.empty());
  return failurePreservesState(r);
}

static bool run_case_second_session_after_interruption() {
  // A task may produce multiple StudySessions (I1): after an interrupted
  // (auto-saved) session the task returns to Paused and a later Start creates
  // a NEW session id.
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  CHECK(s1.ok);
  CHECK(s1.next->active_session->session_id == SessionId{"sess-1"});
  DomainState running = *s1.next;
  b.advance_ms(120'000);  // 2 min before interruption
  const SessionId sid = running.active_session->session_id;
  auto rec = DomainReducer{}.recoverSession(running, sid, RecoveryMode::Intact, b.make());
  CHECK(rec.ok);
  auto end = DomainReducer{}.endRecoveredSession(*rec.next, sid, b.make());
  CHECK(end.ok);
  DomainState paused_task = *end.next;  // task Paused, session cleared
  CHECK(paused_task.tasks[0].status == TaskStatus::Paused);
  auto s2 = DomainReducer{}.reduce(paused_task, intentOf(Intent::StartTask), b.make());
  CHECK(s2.ok);
  CHECK(s2.drafts.size() == 2);
  CHECK(s2.drafts[0].type == EventType::TaskStarted);
  CHECK(s2.drafts[1].type == EventType::StudySessionStarted);
  CHECK(s2.next->active_session->session_id == SessionId{"sess-2"});
  CHECK(s2.next->active_session->task_id == TaskId{"task-1"});
  return true;
}

static bool run_case_recover_intact_keeps_same_session_no_auto_complete() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState running = *s1.next;
  const SessionId sid = running.active_session->session_id;
  auto r = DomainReducer{}.recoverSession(running, sid, RecoveryMode::Intact, b.make());
  CHECK(r.ok);
  CHECK(r.drafts.empty());
  CHECK(r.next->active_session.has_value());
  CHECK(r.next->active_session->session_id == sid);   // same session resumed
  CHECK(r.next->active_session->status == SessionStatus::Running);
  CHECK(r.next->tasks[0].status == TaskStatus::InProgress);  // not auto-completed
  return true;
}

static bool run_case_recover_corrupt_marks_aborted_task_not_completed() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState running = *s1.next;
  const SessionId sid = running.active_session->session_id;
  auto r = DomainReducer{}.recoverSession(running, sid, RecoveryMode::Corrupt, b.make());
  CHECK(r.ok);
  CHECK(!r.next->active_session.has_value());
  CHECK(r.drafts.size() == 1);
  CHECK(r.drafts[0].type == EventType::StudySessionCompleted);
  CHECK(r.drafts[0].payload.at("completion_type") == "aborted");
  // Every completion draft carries its task_id (FIX-10 invariant) so the
  // backend can project/attribute the aborted session.
  CHECK(r.drafts[0].payload.at("task_id") == "task-1");
  CHECK(r.next->tasks[0].status != TaskStatus::Completed);  // never auto-completed
  CHECK(r.next->tasks[0].status == TaskStatus::Paused);     // interrupted, restartable
  return true;
}

static bool run_case_end_recovered_session_auto_saved() {
  CtxBuilder b;
  DomainState s0 = makeState();
  auto s1 = DomainReducer{}.reduce(s0, intentOf(Intent::StartTask), b.make());
  DomainState running = *s1.next;
  b.advance_ms(300'000);  // 5 min before the reboot decision
  const SessionId sid = running.active_session->session_id;
  auto r = DomainReducer{}.endRecoveredSession(running, sid, b.make());
  CHECK(r.ok);
  CHECK(r.drafts.size() == 1);
  CHECK(r.drafts[0].type == EventType::StudySessionCompleted);
  CHECK(r.drafts[0].payload.at("completion_type") == "auto_saved");
  CHECK(r.drafts[0].payload.at("actual_seconds") == "300");
  // FIX-10 invariant: the auto_saved draft carries its task_id.
  CHECK(r.drafts[0].payload.at("task_id") == "task-1");
  CHECK(!r.next->active_session.has_value());
  // Task is NOT auto-completed; back to Paused for a later start.
  CHECK(r.next->tasks[0].status != TaskStatus::Completed);
  CHECK(r.next->tasks[0].status == TaskStatus::Paused);
  return true;
}

static bool run_case_recover_unknown_session_rejected() {
  CtxBuilder b;
  DomainState s0 = makeState();  // no active session
  auto r = DomainReducer{}.recoverSession(s0, SessionId{"nope"}, RecoveryMode::Intact, b.make());
  CHECK(!r.ok);
  CHECK(r.reason == RejectReason::SessionNotActive);
  return failurePreservesState(r);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
static bool run_case_all() {
  CASE(start_ready_to_in_progress_two_drafts);
  CASE(start_event_order_task_before_session);
  CASE(start_on_completed_rejected_idempotent);
  CASE(start_on_in_progress_rejected);
  CASE(start_with_active_session_rejected);
  CASE(start_on_skipped_rejected);
  CASE(start_on_pending_rejected_task_not_ready);
  CASE(start_missing_id_source_rejected);
  CASE(pause_accumulates_and_marks_paused);
  CASE(pause_requires_running);
  CASE(resume_requires_paused);
  CASE(resume_accumulates_pause_seconds);
  CASE(multiple_pause_resume_rounds);
  CASE(clock_backwards_on_pause_rejected);
  CASE(clock_backwards_on_resume_rejected);
  CASE(segment_timeout_pauses_never_completes);
  CASE(after_timeout_user_completes);
  CASE(complete_from_running_manual);
  CASE(complete_from_paused_allowed);
  CASE(duplicate_complete_idempotent);
  CASE(complete_without_active_session_rejected);
  CASE(skip_only_when_ready);
  CASE(duplicate_skip_idempotent);
  CASE(second_session_after_interruption);
  CASE(recover_intact_keeps_same_session_no_auto_complete);
  CASE(recover_corrupt_marks_aborted_task_not_completed);
  CASE(end_recovered_session_auto_saved);
  CASE(recover_unknown_session_rejected);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-STREAM-002 CP1 domain reducer tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
