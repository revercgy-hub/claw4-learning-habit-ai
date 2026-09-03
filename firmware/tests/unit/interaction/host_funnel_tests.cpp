// claw4/firmware/tests/unit/interaction/host_funnel_tests.cpp
// V4 §7/§8 host-funnel integration (WB-LEARNING-V4 P14/P15 acceptance):
// learning.* MCP tools -> CommandDispatcher -> REAL AppCoordinator (domain
// reducer + transactional outbox). This closes the gap between the pure-fake
// unit tests and the device glue: the same funnel the device will use is
// proven against committed domain truth, not a scripted sink.
//
// Deterministic: ids/times are injected through ReducerContext; no network,
// no sleep, no RNG. Compiles/links/runs natively on Windows (exit 0 on pass).

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "application/coordinator.h"
#include "fakes/fake_outbox_storage.h"
#include "interaction/command.h"
#include "interaction/dispatcher.h"
#include "learning_domain/domain_state.h"
#include "learning_domain/reducer.h"
#include "mcp/learning_mcp_host.h"

using namespace claw4::domain;
using namespace claw4::sync;
using namespace claw4::application;
using namespace claw4::interaction;
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
// real-domain harness
// ---------------------------------------------------------------------------
struct CtxGen {
  DeviceId device{"dev-1"};
  ChildId child{"child-1"};
  int64_t epoch = 1756718530;
  int64_t mono = 1'000'000;
  int e = 0;
  int s = 0;

  ReducerContext make() {
    ReducerContext c;
    c.device_id = device;
    c.child_id = child;
    c.now_epoch = epoch;
    c.monotonic_ms = mono;
    c.make_event_id = [this]() { return EventId{"ev-" + std::to_string(++e)}; };
    c.make_session_id = [this]() { return SessionId{"sess-" + std::to_string(++s)}; };
    return c;
  }
  void advance(int64_t ms) { mono += ms; }
};

// The real host glue: an interaction::CommandSink over AppCoordinator.
class CoordSink final : public CommandSink {
 public:
  CoordSink(AppCoordinator& coordinator, CtxGen& ctx)
      : coordinator_(coordinator), ctx_(ctx) {}

  IntentResult emit(const IntentRequest& intent) override {
    const auto tr = coordinator_.dispatchIntent(intent, ctx_.make());
    return tr.intent_result;
  }

 private:
  AppCoordinator& coordinator_;
  CtxGen& ctx_;
};

// The real read-side backend over the coordinator's committed state.
class CoordBackend final : public LearningBackend {
 public:
  CoordBackend(AppCoordinator& coordinator, CtxGen& ctx)
      : coordinator_(coordinator), ctx_(ctx) {}

  DomainState snapshot() const override { return coordinator_.state(); }
  int64_t nowMonotonicMs() const override { return ctx_.mono; }

 private:
  AppCoordinator& coordinator_;
  CtxGen& ctx_;
};

// One funnel per case: coordinator is constructed FIRST, then every addTask()
// merges one task straight into its live committed state (mergeTodayTasks),
// then the MCP host is exercised through the dispatcher.
struct Funnel {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  CoordinatorOptions opts;
  CtxGen ctx;
  AppCoordinator coordinator{storage, reducer, opts};
  CoordSink sink{coordinator, ctx};
  CommandDispatcher dispatcher{sink};
  CoordBackend backend{coordinator, ctx};
  LearningMcpHost host{dispatcher, backend};

  void addTask(const char* id, const char* title, int minutes, TaskStatus status) {
    Task t;
    t.task_id = TaskId{id};
    t.child_id = ChildId{"child-1"};
    t.title = title;
    t.subject = "math";
    t.estimated_minutes = minutes;
    t.status = status;
    t.version = 1;
    coordinator.mergeTodayTasks({t});  // commits the today cache (no events)
  }
};

McpRequest mcp(const char* tool, const char* task_id = nullptr) {
  McpRequest r;
  r.tool = tool;
  if (task_id) r.args["task_id"] = task_id;
  return r;
}

// ---------------------------------------------------------------------------
// cases
// ---------------------------------------------------------------------------
static bool run_case_start_task_commits_through_coordinator() {
  Funnel f;
  f.addTask("t1", "数学口算", 20, TaskStatus::Ready);

  const auto resp = f.host.invoke(mcp(LearningMcpHost::kToolStartTask, "t1"));
  CHECK(resp.ok);
  CHECK(resp.payload.find("\"intent_result\":\"accepted\"") != std::string::npos);

  const auto& st = f.coordinator.state();
  CHECK(st.tasks.size() == 1);
  CHECK(st.tasks[0].status == TaskStatus::InProgress);
  CHECK(st.active_session.has_value());
  CHECK(f.coordinator.pendingCount() == 2);  // TaskStarted + StudySessionStarted
  return true;
}

static bool run_case_current_task_and_remaining_read_live_state() {
  Funnel f;
  f.addTask("t1", "数学口算", 30, TaskStatus::Ready);
  CHECK(f.host.invoke(mcp(LearningMcpHost::kToolStartTask, "t1")).ok);

  auto cur = f.host.invoke(mcp(LearningMcpHost::kToolGetCurrentTask));
  CHECK(cur.ok);
  CHECK(cur.payload.find("\"active\":true") != std::string::npos);
  CHECK(cur.payload.find("\"task_status\":\"in_progress\"") != std::string::npos);

  auto rem = f.host.invoke(mcp(LearningMcpHost::kToolGetRemainingTime));
  CHECK(rem.ok);
  // 30 min planned, segment just started -> full 1,800,000 ms remaining.
  CHECK(rem.payload.find("\"remaining_ms\":1800000") != std::string::npos);
  CHECK(rem.payload.find("\"running\":true") != std::string::npos);
  return true;
}

static bool run_case_pause_resume_through_domain_state_machine() {
  Funnel f;
  f.addTask("t1", "数学口算", 20, TaskStatus::Ready);
  CHECK(f.host.invoke(mcp(LearningMcpHost::kToolStartTask, "t1")).ok);

  // 5 minutes into the segment
  f.ctx.advance(300'000);
  CHECK(f.host.invoke(mcp(LearningMcpHost::kToolPauseTask, "t1")).ok);
  CHECK(f.coordinator.state().tasks[0].status == TaskStatus::Paused);
  CHECK(f.coordinator.state().active_session->status == SessionStatus::Paused);

  CHECK(f.host.invoke(mcp(LearningMcpHost::kToolResumeTask, "t1")).ok);
  CHECK(f.coordinator.state().tasks[0].status == TaskStatus::InProgress);
  CHECK(f.coordinator.state().active_session->status == SessionStatus::Running);
  return true;
}

static bool run_case_mcp_complete_gated_then_confirmed_via_domain() {
  Funnel f;
  f.addTask("t1", "数学口算", 20, TaskStatus::Ready);
  CHECK(f.host.invoke(mcp(LearningMcpHost::kToolStartTask, "t1")).ok);

  const auto resp = f.host.invoke(mcp(LearningMcpHost::kToolRequestCompleteTask, "t1"));
  CHECK(resp.ok);
  CHECK(resp.payload.find("\"confirmation\":\"pending\"") != std::string::npos);
  // AI path must NOT have mutated the domain.
  CHECK(f.coordinator.state().tasks[0].status == TaskStatus::InProgress);

  // The child physically confirms -> exactly one real Complete transition.
  const auto cr = f.dispatcher.confirmPendingComplete();
  CHECK(cr.status == DispatchStatus::Emitted);
  const auto& st = f.coordinator.state();
  CHECK(st.tasks[0].status == TaskStatus::Completed);
  // The reducer clears active_session once the session is committed (the Done
  // page summary is app-shell runtime state, never domain truth).
  CHECK(!st.active_session.has_value());

  auto cur = f.host.invoke(mcp(LearningMcpHost::kToolGetCurrentTask));
  CHECK(cur.ok);
  CHECK(cur.payload.find("\"active\":false") != std::string::npos);
  return true;
}

static bool run_case_start_busy_task_domain_rejects() {
  Funnel f;
  f.addTask("t1", "数学口算", 20, TaskStatus::Ready);
  f.addTask("t2", "英语朗读", 15, TaskStatus::Ready);
  CHECK(f.host.invoke(mcp(LearningMcpHost::kToolStartTask, "t1")).ok);

  // Starting a second task while one is active is rejected by the domain.
  const auto resp = f.host.invoke(mcp(LearningMcpHost::kToolStartTask, "t2"));
  CHECK(resp.ok);
  CHECK(resp.payload.find("\"intent_result\":\"rejected\"") != std::string::npos);
  CHECK(f.coordinator.state().tasks[0].status == TaskStatus::InProgress);
  CHECK(f.coordinator.state().tasks[1].status == TaskStatus::Ready);
  return true;
}

static bool run_case_pending_task_not_startable_rejected() {
  Funnel f;
  // Pending == future local date; never startable from the device (task.h).
  f.addTask("t3", "明天任务", 10, TaskStatus::Pending);

  const auto resp = f.host.invoke(mcp(LearningMcpHost::kToolStartTask, "t3"));
  CHECK(resp.ok);
  CHECK(resp.payload.find("\"intent_result\":\"rejected\"") != std::string::npos);
  CHECK(f.coordinator.state().tasks[0].status == TaskStatus::Pending);
  CHECK(!f.coordinator.state().active_session.has_value());
  return true;
}

static bool run_case_all() {
  CASE(start_task_commits_through_coordinator);
  CASE(current_task_and_remaining_read_live_state);
  CASE(pause_resume_through_domain_state_machine);
  CASE(mcp_complete_gated_then_confirmed_via_domain);
  CASE(start_busy_task_domain_rejects);
  CASE(pending_task_not_startable_rejected);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-LEARNING-V4 P14/P15 host funnel (real domain) tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
