// claw4/firmware/tests/unit/metalio/restart_recovery_tests.cpp
// WB-LEARNING-V4-L1 — device-restart funnel reproduction attempt (HANDOFF §4b).
// Simulates the device sequence on the host with a shared FakeDisk ("NVS"):
//   seed demo today snapshot -> Start demo-math (Accepted, pending=2)
//   -> simulate reboot by re-instantiating the coordinator over the SAME disk
//   -> verify recovered state -> Pause / Resume / Complete each expect Accepted.
// If Pause returns PersistFailed here, the defect is domain/outbox-level (the
// L1c device failure is NOT NVS-specific). If all pass, the defect is
// device-adapter-specific (NVS storage / context / timing).
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "application/coordinator.h"
#include "fakes/fake_outbox_storage.h"
#include "learning_domain/domain_state.h"
#include "learning_domain/ids.h"
#include "learning_domain/intents.h"
#include "learning_domain/reducer.h"
#include "learning_domain/task.h"

namespace {

using claw4::application::AppCoordinator;
using claw4::domain::ChildId;
using claw4::domain::DeviceId;
using claw4::domain::DomainReducer;
using claw4::domain::EventId;
using claw4::domain::Intent;
using claw4::domain::IntentRequest;
using claw4::domain::IntentResult;
using claw4::domain::ReducerContext;
using claw4::domain::SessionId;
using claw4::domain::Task;
using claw4::domain::TaskId;
using claw4::domain::TaskStatus;
using claw4::sync::FakeDisk;
using claw4::sync::FakeOutboxStorage;

int g_fail = 0;
#define CHECK(x)                                    \
  do {                                              \
    if (!(x)) {                                     \
      std::printf("FAIL %s:%d: %s\n", __FILE__,     \
                  __LINE__, #x);                    \
      ++g_fail;                                     \
    }                                               \
  } while (0)

struct FakeClock {
  int64_t ms = 100000;  // deterministic monotonic
  void advance(int64_t by) { ms += by; }
};

Task MakeDemoMath() {
  Task t;
  t.task_id = TaskId{"demo-math-001"};
  t.child_id = ChildId{"child-1"};
  t.title = "口算练习";
  t.subject = "math";
  t.estimated_minutes = 20;
  t.priority = "high";
  t.status = TaskStatus::Ready;
  t.scheduled_date = "2026-09-04";
  t.version = 1;
  return t;
}

ReducerContext Ctx(FakeClock& c, int& ev_seq, int& sess_seq) {
  ReducerContext r;
  r.device_id = DeviceId{"dev-claw4-l1"};
  r.child_id = ChildId{"child-1"};
  r.now_epoch = c.ms / 1000;
  r.monotonic_ms = c.ms;
  r.make_event_id = [&] { return EventId{"ev-" + std::to_string(++ev_seq)}; };
  r.make_session_id = [&] {
    return SessionId{"sess-" + std::to_string(++sess_seq)};
  };
  return r;
}

bool RunRestartPauseScenario() {
  auto disk = std::make_shared<FakeDisk>();
  DomainReducer reducer;
  FakeClock clock;
  int ev_seq = 0;
  int sess_seq = 0;

  // ---- boot 1: seed + start ----
  {
    FakeOutboxStorage storage(disk);
    AppCoordinator c(storage, reducer);
    CHECK(c.applyTodaySnapshot({MakeDemoMath()}));  // seed snapshot
    CHECK(c.state().tasks.size() == 1);
    IntentRequest start;
    start.intent = Intent::StartTask;
    start.task_id = TaskId{"demo-math-001"};
    const auto r1 = c.dispatchIntent(start, Ctx(clock, ev_seq, sess_seq));
    CHECK(r1.ok);
    CHECK(r1.intent_result == IntentResult::Accepted);
    CHECK(c.state().active_session.has_value());
    CHECK(c.pendingCount() == 2);  // TaskStarted + StudySessionStarted
    clock.advance(5000);           // 5 s of focus
  }
  // disk destructs storage but disk (state) survives -> "reboot"

  // ---- boot 2: recovered state, then Pause/Resume/Complete ----
  {
    FakeOutboxStorage storage(disk);
    AppCoordinator c(storage, reducer);
    CHECK(c.state().tasks.size() == 1);
    CHECK(c.state().tasks[0].status == TaskStatus::InProgress);
    CHECK(c.state().active_session.has_value());
    CHECK(c.pendingCount() == 2);

    IntentRequest pause;
    pause.intent = Intent::Pause;
    pause.task_id = TaskId{"demo-math-001"};
    const auto rp = c.dispatchIntent(pause, Ctx(clock, ev_seq, sess_seq));
    CHECK(rp.ok);
    CHECK(rp.intent_result == IntentResult::Accepted);
    if (!rp.ok) {
      std::printf("REPRODUCED: Pause after restart -> ok=%d intent=%d\n",
                  rp.ok ? 1 : 0,
                  static_cast<int>(rp.intent_result));
    }
    CHECK(c.pendingCount() == 3);

    IntentRequest resume;
    resume.intent = Intent::Resume;
    resume.task_id = TaskId{"demo-math-001"};
    const auto rr = c.dispatchIntent(resume, Ctx(clock, ev_seq, sess_seq));
    CHECK(rr.ok);
    CHECK(rr.intent_result == IntentResult::Accepted);
    clock.advance(3000);

    IntentRequest complete;
    complete.intent = Intent::Complete;
    complete.task_id = TaskId{"demo-math-001"};
    const auto rc = c.dispatchIntent(complete, Ctx(clock, ev_seq, sess_seq));
    CHECK(rc.ok);
    CHECK(rc.intent_result == IntentResult::Accepted);
    CHECK(c.state().tasks[0].status == TaskStatus::Completed);
    CHECK(c.pendingCount() == 6);  // start2 + pause1 + resume1 + complete2
  }
  return g_fail == 0;
}

}  // namespace

int main() {
  RunRestartPauseScenario();
  if (g_fail == 0) {
    std::printf(
        "restart_recovery_tests: all PASS (restart->Pause/Resume/Complete OK "
        "on host -> defect NOT reproduced at domain level)\n");
    return 0;
  }
  std::printf("restart_recovery_tests: %d FAILURES\n", g_fail);
  return 1;
}
