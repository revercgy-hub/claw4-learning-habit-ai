// claw4/firmware/tests/contracts/contract_tests.cpp
// Compile-time interface contract checks for the MVP interface skeleton.
//
// This file is ONLY compiled with -fsyntax-only (or equivalent) using a
// cross compiler; it is NOT linked and no unit tests are claimed to have run
// on this host (see WB-STREAM-001 CP1 constraints). It proves that every
// interface header is self-contained, that the value types are usable, and
// that the pure-domain headers carry zero hardware dependencies.

#include <optional>
#include <type_traits>

#include "assistant/command.h"
#include "assistant/command_router.h"
#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"
#include "learning_domain/ids.h"
#include "learning_domain/intents.h"
#include "learning_domain/services.h"
#include "learning_domain/study_session.h"
#include "learning_domain/task.h"
#include "sync/batch_result.h"
#include "sync/error_class.h"
#include "sync/event_sink.h"
#include "sync/sync_client.h"
#include "ui/intent_sink.h"
#include "ui/view_state.h"
#include "telemetry/logger.h"

namespace claw4 {
namespace contracts {

using namespace claw4::domain;
using namespace claw4::sync;

// --- enum value stability (contract anchors) ---
static_assert(static_cast<int>(TaskStatus::Pending) == 0);
static_assert(static_cast<int>(TaskStatus::Completed) == 4);
static_assert(static_cast<int>(SessionStatus::Aborted) == 4);
static_assert(static_cast<int>(CompletionType::AutoSaved) == 2);
static_assert(static_cast<int>(CompletionType::Aborted) == 3);
// No Timeout in completion_type (CR-WB002-03/09): the enum has exactly 4 values.
static_assert(static_cast<int>(CompletionType::Aborted) + 1 == 4);

static_assert(static_cast<int>(EventType::StudySessionCompleted) == 9);
static_assert(static_cast<int>(EventType::SyncFailed) == 10);
static_assert(static_cast<int>(EventType::SyncRecovered) == 11);

static_assert(static_cast<int>(EventOutcome::Accepted) == 0);
static_assert(static_cast<int>(EventOutcome::Duplicate) == 1);
static_assert(static_cast<int>(EventOutcome::Conflict) == 2);
static_assert(static_cast<int>(EventOutcome::Rejected) == 3);
static_assert(static_cast<int>(EventOutcome::Gap) == 4);

static_assert(static_cast<int>(SyncErrorClass::Auth) == 1);

// --- ID types are default-constructible value types ---
static_assert(std::is_default_constructible_v<TaskId>);
static_assert(std::is_default_constructible_v<SessionId>);
static_assert(std::is_default_constructible_v<ChildId>);
static_assert(std::is_default_constructible_v<DeviceId>);
static_assert(std::is_default_constructible_v<EventId>);

// --- value types are copyable/movable aggregates ---
static_assert(std::is_copy_constructible_v<Task>);
static_assert(std::is_copy_constructible_v<StudySession>);
static_assert(std::is_copy_constructible_v<DeviceEvent>);
static_assert(std::is_copy_constructible_v<DomainState>);
static_assert(std::is_copy_constructible_v<BatchSyncResult>);
static_assert(std::is_copy_constructible_v<SyncClient::Request>);

// --- interfaces are abstract, mocks are concrete ---
static_assert(std::is_abstract_v<TaskService>);
static_assert(std::is_abstract_v<SessionService>);
static_assert(std::is_abstract_v<TimerService>);
static_assert(std::is_abstract_v<EventSink>);
static_assert(std::is_abstract_v<SyncClient>);
static_assert(std::is_abstract_v<ui::IntentSink>);
static_assert(std::is_abstract_v<assistant::CommandRouter>);
static_assert(std::is_abstract_v<telemetry::Logger>);

// --- mock implementations prove the interfaces are implementable ---
class MockTaskService final : public TaskService {
 public:
  IntentResult start(const IntentRequest&, const TaskId&) override { return IntentResult::Accepted; }
  IntentResult pause(const TaskId&) override { return IntentResult::Accepted; }
  IntentResult resume(const TaskId&) override { return IntentResult::Accepted; }
  IntentResult complete(const TaskId&) override { return IntentResult::Accepted; }
  IntentResult skip(const TaskId&) override { return IntentResult::Accepted; }
};

class MockSessionService final : public SessionService {
 public:
  IntentResult startSession(const IntentRequest&, const TaskId&) override { return IntentResult::Accepted; }
  IntentResult pauseSession(const SessionId&) override { return IntentResult::Accepted; }
  IntentResult resumeSession(const SessionId&) override { return IntentResult::Accepted; }
  IntentResult endSession(const SessionId&, CompletionType) override { return IntentResult::Accepted; }
};

class MockTimerService final : public TimerService {
 public:
  void onTick(int64_t) override {}
  void onSegmentTimeout(const SessionId&) override {}
};

class MockEventSink final : public EventSink {
 public:
  bool persist(const DeviceEvent&) override { return true; }
};

class MockSyncClient final : public SyncClient {
 public:
  Response syncBatch(const Request&) override {
    Response r;
    r.error_class = SyncErrorClass::None;
    return r;
  }
};

class MockIntentSink final : public ui::IntentSink {
 public:
  IntentResult emit(const IntentRequest&) override { return IntentResult::Accepted; }
};

class MockCommandRouter final : public assistant::CommandRouter {
 public:
  std::optional<IntentRequest> route(const assistant::Command& c) const override {
    if (c == assistant::Command::Unknown) return std::nullopt;
    return IntentRequest{};
  }
};

class MockLogger final : public telemetry::Logger {
 public:
  void log(telemetry::LogTag, std::string_view) override {}
  void logMetric(std::string_view, int64_t) override {}
};

// The mocks are non-abstract, proving the pure interfaces are implementable
// with zero hardware dependency.
static_assert(!std::is_abstract_v<MockTaskService>);
static_assert(!std::is_abstract_v<MockSessionService>);
static_assert(!std::is_abstract_v<MockTimerService>);
static_assert(!std::is_abstract_v<MockEventSink>);
static_assert(!std::is_abstract_v<MockSyncClient>);
static_assert(!std::is_abstract_v<MockIntentSink>);
static_assert(!std::is_abstract_v<MockCommandRouter>);
static_assert(!std::is_abstract_v<MockLogger>);

// --- value types are usable (ordinary functions; NOT run on this host) ---
bool exerciseValueTypes() {
  Task t;
  t.task_id.value = "t-1";
  t.child_id.value = "c-1";
  t.status = TaskStatus::Ready;

  DeviceEvent ev;
  ev.event_id.value = "e-1";
  ev.sequence = 42;
  ev.payload["session_id"] = "s-1";

  SyncClient::Request req;
  req.last_acked_sequence = 41;
  req.events.push_back(ev);

  BatchSyncResult b;
  b.last_acked_sequence = 42;
  PerEventResult r;
  r.outcome = EventOutcome::Duplicate;
  b.results.push_back(r);

  MockSyncClient client;
  auto resp = client.syncBatch(req);
  return t.status == TaskStatus::Ready && resp.batch.last_acked_sequence == 0 &&
         b.results[0].outcome == EventOutcome::Duplicate;
}

// Reference the function so it participates in semantic analysis even under
// -fsyntax-only (a definition is enough to prove type correctness).
using ExerciseFn = bool (*)();
inline ExerciseFn exercise_fn = &exerciseValueTypes;

}  // namespace contracts
}  // namespace claw4
