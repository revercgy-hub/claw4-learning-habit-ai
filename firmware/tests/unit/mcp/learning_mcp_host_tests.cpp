// claw4/firmware/tests/unit/mcp/learning_mcp_host_tests.cpp
// Host unit tests for the 8 learning.* MCP tools (WB-LEARNING-V4 P15).
//
// Compiles, links and RUNS natively on Windows. Explicit case/assert counter,
// non-zero exit on failure. Mock transport = direct invoke() calls; mutations
// are observed through the shared fake CommandSink under the dispatcher.

#include <cstdint>
#include <cstdio>
#include <string>

#include "fakes/fake_command_sink.h"
#include "fakes/fake_learning_backend.h"
#include "interaction/dispatcher.h"
#include "learning_domain/domain_state.h"
#include "learning_domain/task.h"
#include "mcp/learning_mcp_host.h"

using namespace claw4::domain;
using namespace claw4::mcp;

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
struct Harness {
  claw4::interaction::FakeCommandSink sink;
  claw4::interaction::CommandDispatcher dispatcher{sink};
  FakeLearningBackend backend;
  LearningMcpHost host{dispatcher, backend};

  Task& addTask(const char* id, const char* title, const char* subject,
                int minutes, TaskStatus status) {
    Task t;
    t.task_id.value = id;
    t.child_id.value = "child-1";
    t.title = title;
    t.subject = subject;
    t.estimated_minutes = minutes;
    t.status = status;
    t.scheduled_date = "2026-09-03";
    backend.state.tasks.push_back(t);
    return backend.state.tasks.back();
  }

  void startRunningSession(const char* task_id, int planned_minutes,
                           int64_t segment_start_ms) {
    StudySession s;
    s.session_id.value = "sess-1";
    s.task_id.value = task_id;
    s.child_id.value = "child-1";
    s.device_id.value = "dev-1";
    s.planned_minutes = planned_minutes;
    s.status = SessionStatus::Running;
    s.segment_start_monotonic_ms = segment_start_ms;
    backend.state.active_session = s;
  }

  McpRequest req(const char* tool) {
    McpRequest r;
    r.tool = tool;
    return r;
  }
};

// ---------------------------------------------------------------------------
// query tools
// ---------------------------------------------------------------------------
static bool run_case_get_today_tasks_lists_snapshot() {
  Harness h;
  h.addTask("t1", "数学口算", "math", 20, TaskStatus::Ready);
  h.addTask("t2", "英语朗读", "english", 15, TaskStatus::Completed);
  const auto r = h.host.invoke(h.req(LearningMcpHost::kToolGetTodayTasks));
  CHECK(r.ok);
  CHECK(r.payload.find("\"count\":2") != std::string::npos);
  CHECK(r.payload.find("\"title\":\"数学口算\"") != std::string::npos);
  CHECK(r.payload.find("\"status\":\"ready\"") != std::string::npos);
  CHECK(r.payload.find("\"status\":\"completed\"") != std::string::npos);
  return true;
}

static bool run_case_get_today_tasks_empty() {
  Harness h;
  const auto r = h.host.invoke(h.req(LearningMcpHost::kToolGetTodayTasks));
  CHECK(r.ok);
  CHECK(r.payload.find("\"count\":0") != std::string::npos);
  return true;
}

static bool run_case_get_current_task_active() {
  Harness h;
  h.addTask("t1", "数学口算", "math", 20, TaskStatus::InProgress);
  h.startRunningSession("t1", 20, 1'000);
  const auto r = h.host.invoke(h.req(LearningMcpHost::kToolGetCurrentTask));
  CHECK(r.ok);
  CHECK(r.payload.find("\"active\":true") != std::string::npos);
  CHECK(r.payload.find("\"task_id\":\"t1\"") != std::string::npos);
  CHECK(r.payload.find("\"title\":\"数学口算\"") != std::string::npos);
  CHECK(r.payload.find("\"session_status\":\"running\"") != std::string::npos);
  return true;
}

static bool run_case_get_current_task_inactive() {
  Harness h;
  const auto r = h.host.invoke(h.req(LearningMcpHost::kToolGetCurrentTask));
  CHECK(r.ok);
  CHECK(r.payload == R"({"active":false})");
  return true;
}

static bool run_case_get_remaining_time_running_decreases_with_now() {
  Harness h;
  h.addTask("t1", "数学口算", "math", 30, TaskStatus::InProgress);
  h.startRunningSession("t1", 30, 1'000);

  h.backend.now_monotonic_ms = 1'000;  // just started: full 30 min left
  auto r0 = h.host.invoke(h.req(LearningMcpHost::kToolGetRemainingTime));
  CHECK(r0.ok);
  CHECK(r0.payload.find("\"active\":true") != std::string::npos);
  CHECK(r0.payload.find("\"remaining_ms\":1800000") != std::string::npos);
  CHECK(r0.payload.find("\"running\":true") != std::string::npos);

  h.backend.now_monotonic_ms = 601'000;  // 10 min into the segment
  auto r1 = h.host.invoke(h.req(LearningMcpHost::kToolGetRemainingTime));
  CHECK(r1.ok);
  CHECK(r1.payload.find("\"remaining_ms\":1200000") != std::string::npos);
  return true;
}

static bool run_case_get_remaining_time_inactive() {
  Harness h;
  const auto r = h.host.invoke(h.req(LearningMcpHost::kToolGetRemainingTime));
  CHECK(r.ok);
  CHECK(r.payload == R"({"active":false})");
  return true;
}

static bool run_case_get_today_progress_rule() {
  Harness h;
  h.addTask("t1", "A", "math", 10, TaskStatus::Completed);
  h.addTask("t2", "B", "math", 10, TaskStatus::Completed);
  h.addTask("t3", "C", "math", 10, TaskStatus::InProgress);
  h.addTask("t4", "D", "math", 10, TaskStatus::Ready);
  h.addTask("t5", "E", "math", 10, TaskStatus::Skipped);
  h.addTask("t6", "F", "math", 10, TaskStatus::Pending);  // excluded (future)
  const auto r = h.host.invoke(h.req(LearningMcpHost::kToolGetTodayProgress));
  CHECK(r.ok);
  CHECK(r.payload.find("\"completed\":2") != std::string::npos);
  CHECK(r.payload.find("\"total\":5") != std::string::npos);  // pending excluded
  CHECK(r.payload.find("\"percent\":40") != std::string::npos);
  return true;
}

static bool run_case_get_today_progress_empty_is_zero() {
  Harness h;
  const auto r = h.host.invoke(h.req(LearningMcpHost::kToolGetTodayProgress));
  CHECK(r.ok);
  CHECK(r.payload == R"({"completed":0,"total":0,"percent":0})");
  return true;
}

// ---------------------------------------------------------------------------
// mutation tools (observed through the shared sink)
// ---------------------------------------------------------------------------
static bool run_case_start_task_emits_start_intent() {
  Harness h;
  h.addTask("t1", "数学口算", "math", 20, TaskStatus::Ready);
  auto r = h.req(LearningMcpHost::kToolStartTask);
  r.args["task_id"] = "t1";
  const auto resp = h.host.invoke(r);
  CHECK(resp.ok);
  CHECK(resp.payload.find("\"intent_result\":\"accepted\"") != std::string::npos);
  CHECK(h.sink.emit_count() == 1);
  CHECK(h.sink.emitted().front().intent == Intent::StartTask);
  CHECK(h.sink.emitted().front().task_id->value == "t1");
  return true;
}

static bool run_case_pause_resume_emit_correct_intents() {
  Harness h;
  h.addTask("t1", "数学口算", "math", 20, TaskStatus::InProgress);
  h.startRunningSession("t1", 20, 1'000);

  auto p = h.req(LearningMcpHost::kToolPauseTask);
  p.args["task_id"] = "t1";
  CHECK(h.host.invoke(p).ok);
  CHECK(h.sink.emitted().back().intent == Intent::Pause);

  auto r = h.req(LearningMcpHost::kToolResumeTask);
  r.args["task_id"] = "t1";
  CHECK(h.host.invoke(r).ok);
  CHECK(h.sink.emitted().back().intent == Intent::Resume);
  CHECK(h.sink.emit_count() == 2);
  return true;
}

static bool run_case_pause_without_task_arg_uses_active_session() {
  Harness h;
  h.addTask("t1", "数学口算", "math", 20, TaskStatus::InProgress);
  h.startRunningSession("t1", 20, 1'000);
  const auto resp = h.host.invoke(h.req(LearningMcpHost::kToolPauseTask));
  CHECK(resp.ok);
  CHECK(h.sink.emit_count() == 1);
  CHECK(h.sink.emitted().front().intent == Intent::Pause);
  CHECK(h.sink.emitted().front().task_id->value == "t1");
  return true;
}

static bool run_case_request_complete_never_completes_directly() {
  Harness h;
  h.addTask("t1", "数学口算", "math", 20, TaskStatus::InProgress);
  h.startRunningSession("t1", 20, 1'000);
  auto r = h.req(LearningMcpHost::kToolRequestCompleteTask);
  r.args["task_id"] = "t1";
  const auto resp = h.host.invoke(r);
  CHECK(resp.ok);
  CHECK(resp.payload.find("\"confirmation\":\"pending\"") != std::string::npos);
  CHECK(h.sink.emit_count() == 0);  // AI never emits Complete (V4 §8)

  // the child confirms physically -> exactly one Complete via Touch semantics
  const auto cr = h.dispatcher.confirmPendingComplete();
  CHECK(cr.status == claw4::interaction::DispatchStatus::Emitted);
  CHECK(h.sink.emit_count() == 1);
  CHECK(h.sink.emitted().front().intent == Intent::Complete);
  CHECK(h.sink.emitted().front().task_id->value == "t1");
  return true;
}

static bool run_case_missing_task_id_rejected() {
  Harness h;
  h.addTask("t1", "A", "math", 10, TaskStatus::Ready);
  const auto resp = h.host.invoke(h.req(LearningMcpHost::kToolStartTask));
  CHECK(!resp.ok);
  CHECK(resp.error.find("missing_arg:task_id") != std::string::npos);
  CHECK(h.sink.emit_count() == 0);
  return true;
}

static bool run_case_unknown_task_rejected() {
  Harness h;
  auto r = h.req(LearningMcpHost::kToolStartTask);
  r.args["task_id"] = "ghost";
  const auto resp = h.host.invoke(r);
  CHECK(!resp.ok);
  CHECK(resp.error.find("task_not_found:ghost") != std::string::npos);
  CHECK(h.sink.emit_count() == 0);
  return true;
}

static bool run_case_unknown_tool_rejected() {
  Harness h;
  const auto resp = h.host.invoke(h.req("learning.dance"));
  CHECK(!resp.ok);
  CHECK(resp.error.find("unknown_tool:") != std::string::npos);
  CHECK(h.sink.emit_count() == 0);
  return true;
}

static bool run_case_pause_without_session_or_arg_error() {
  // No active session and no task_id -> the host cannot resolve a target.
  Harness h;
  h.addTask("t1", "数学口算", "math", 20, TaskStatus::InProgress);  // task exists
  const auto resp = h.host.invoke(h.req(LearningMcpHost::kToolPauseTask));
  CHECK(!resp.ok);
  CHECK(resp.error.find("missing_arg:task_id") != std::string::npos);
  CHECK(h.sink.emit_count() == 0);
  return true;
}

static bool run_case_all() {
  CASE(get_today_tasks_lists_snapshot);
  CASE(get_today_tasks_empty);
  CASE(get_current_task_active);
  CASE(get_current_task_inactive);
  CASE(get_remaining_time_running_decreases_with_now);
  CASE(get_remaining_time_inactive);
  CASE(get_today_progress_rule);
  CASE(get_today_progress_empty_is_zero);
  CASE(start_task_emits_start_intent);
  CASE(pause_resume_emit_correct_intents);
  CASE(pause_without_task_arg_uses_active_session);
  CASE(request_complete_never_completes_directly);
  CASE(missing_task_id_rejected);
  CASE(unknown_task_rejected);
  CASE(unknown_tool_rejected);
  CASE(pause_without_session_or_arg_error);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-LEARNING-V4 P15 learning MCP host tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
