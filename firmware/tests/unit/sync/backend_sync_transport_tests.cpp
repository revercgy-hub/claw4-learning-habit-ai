// CODEX-APP-FIRST-001 AF2 — coordinator -> BackendClient integration seam.
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "application/coordinator.h"
#include "fakes/fake_outbox_storage.h"
#include "learning_domain/reducer.h"
#include "sync/backend_sync_transport.h"

namespace {

using claw4::application::AppCoordinator;
using claw4::application::SyncOutcome;
using claw4::domain::ChildId;
using claw4::domain::DeviceId;
using claw4::domain::EventId;
using claw4::domain::Intent;
using claw4::domain::IntentRequest;
using claw4::domain::ReducerContext;
using claw4::domain::Task;
using claw4::domain::TaskId;
using claw4::domain::TaskStatus;
using claw4::domain::DomainReducer;
using claw4::sync::BackendClient;
using claw4::sync::BackendSyncTransport;
using claw4::sync::HttpResponse;
using claw4::sync::HttpTransport;
using claw4::sync::SyncErrorClass;

int g_fail = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    if (!(x)) {                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);         \
      ++g_fail;                                                         \
    }                                                                   \
  } while (0)

class FakeHttp final : public HttpTransport {
 public:
  std::vector<HttpResponse> responses;
  std::vector<std::string> bodies;

  HttpResponse request(const std::string&, const std::string&,
                       const std::vector<std::pair<std::string, std::string>>&,
                       const std::string& body, int64_t) override {
    bodies.push_back(body);
    if (responses.empty()) return HttpResponse{false, 0, {}};
    const auto response = responses.front();
    responses.erase(responses.begin());
    return response;
  }
};

Task ReadyTask() {
  Task task;
  task.task_id = TaskId{"task-1"};
  task.child_id = ChildId{"child-1"};
  task.title = "AF2 task";
  task.status = TaskStatus::Ready;
  task.version = 1;
  return task;
}

struct Env {
  std::shared_ptr<claw4::sync::FakeDisk> disk =
      std::make_shared<claw4::sync::FakeDisk>();
  claw4::sync::FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  int event_id = 0;
  ReducerContext context() {
    ReducerContext context;
    context.device_id = DeviceId{"dev-af2"};
    context.child_id = ChildId{"child-1"};
    context.now_epoch = 1700000000;
    context.monotonic_ms = 1000;
    context.make_event_id = [this]() { return EventId{"af2-" + std::to_string(++event_id)}; };
    context.make_session_id = []() { return claw4::domain::SessionId{"sess-af2"}; };
    return context;
  }
};

void RunCoordinatorBatch() {
  Env env;
  AppCoordinator coordinator(env.storage, env.reducer);
  CHECK(coordinator.applyTodaySnapshot({ReadyTask()}));
  IntentRequest start;
  start.intent = Intent::StartTask;
  start.task_id = TaskId{"task-1"};
  CHECK(coordinator.dispatchIntent(start, env.context()).ok);
  CHECK(coordinator.pendingCount() == 2);

  FakeHttp http;
  http.responses.push_back({true, 200,
      "{\"last_acked_sequence\":2,\"server_time\":1700000001,\"results\":["
      "{\"event_id\":\"af2-1\",\"sequence\":1,\"status\":\"accepted\",\"http_status\":200},"
      "{\"event_id\":\"af2-2\",\"sequence\":2,\"status\":\"accepted\",\"http_status\":200}]}"});
  BackendClient client(http, "http://backend");
  BackendSyncTransport transport(client);
  CHECK(coordinator.runSyncOnce(transport, [] { return false; }) ==
        SyncOutcome::Synced);
  CHECK(coordinator.pendingCount() == 0);
  CHECK(coordinator.lastAcked() == 2);
  CHECK(http.bodies.size() == 1);
  CHECK(http.bodies[0].find("\"sequence\":1") != std::string::npos);
}

void RunNetworkRetryAndDuplicateConvergence() {
  Env env;
  AppCoordinator coordinator(env.storage, env.reducer);
  CHECK(coordinator.applyTodaySnapshot({ReadyTask()}));
  IntentRequest start;
  start.intent = Intent::StartTask;
  start.task_id = TaskId{"task-1"};
  CHECK(coordinator.dispatchIntent(start, env.context()).ok);

  FakeHttp http;
  http.responses.push_back({false, 0, {}});
  http.responses.push_back({true, 200,
      "{\"last_acked_sequence\":2,\"server_time\":1700000002,\"results\":["
      "{\"event_id\":\"af2-1\",\"sequence\":1,\"status\":\"duplicate\",\"http_status\":200},"
      "{\"event_id\":\"af2-2\",\"sequence\":2,\"status\":\"duplicate\",\"http_status\":200}]}"});
  BackendClient client(http, "http://backend");
  BackendSyncTransport transport(client);
  CHECK(coordinator.runSyncOnce(transport, [] { return false; }) ==
        SyncOutcome::Backoff);
  CHECK(coordinator.pendingCount() == 2);
  CHECK(coordinator.runSyncOnce(transport, [] { return false; }) ==
        SyncOutcome::Synced);
  CHECK(coordinator.pendingCount() == 0);
  CHECK(coordinator.lastAcked() == 2);
  CHECK(http.bodies.size() == 2);
  CHECK(http.bodies[0] == http.bodies[1]);
}

}  // namespace

int main() {
  RunCoordinatorBatch();
  RunNetworkRetryAndDuplicateConvergence();
  if (g_fail == 0) {
    std::printf("backend_sync_transport_tests: all PASS\n");
    return 0;
  }
  std::printf("backend_sync_transport_tests: %d FAILURES\n", g_fail);
  return 1;
}
