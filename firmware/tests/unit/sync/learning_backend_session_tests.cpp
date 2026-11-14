#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "application/coordinator.h"
#include "learning_domain/reducer.h"
#include "ports/clock_port.h"
#include "sync/http_transport.h"
#include "sync/learning_backend_session.h"
#include "../../fakes/fake_outbox_storage.h"

namespace {

using namespace claw4;
using sync::HttpResponse;
using sync::HttpTransport;

int g_fail = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    if (!(x)) {                                                         \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);        \
      ++g_fail;                                                         \
    }                                                                   \
  } while (0)

class FakeClock final : public ports::ClockPort {
 public:
  int64_t epochSeconds() override { return 1700000000; }
  int64_t monotonicMs() override { return 1000; }
  bool isTimeSynced() override { return true; }
};

class FakeHttp final : public HttpTransport {
 public:
  std::vector<HttpResponse> responses;
  int requests = 0;

  HttpResponse request(const std::string&, const std::string&,
                       const std::vector<std::pair<std::string, std::string>>&,
                       const std::string&, int64_t) override {
    ++requests;
    if (responses.empty()) return {};
    const auto response = responses.front();
    responses.erase(responses.begin());
    return response;
  }
};

void RunConfiguredCycle() {
  auto disk = std::make_shared<sync::FakeDisk>();
  sync::FakeOutboxStorage storage(disk);
  domain::DomainReducer reducer;
  application::AppCoordinator app(storage, reducer);
  auto http = std::make_unique<FakeHttp>();
  auto* raw = http.get();
  raw->responses = {
      {true, 200, "{\"challenge_id\":\"ch-1\",\"nonce\":\"n-1\",\"expires_at\":1700000100}"},
      {true, 200, "{\"access_token\":\"tok-1\",\"expires_in\":3600}"},
      {true, 200, "{\"date\":\"2026-09-06\",\"tasks\":[{\"task_id\":\"task-1\",\"title\":\"Math\",\"subject\":\"math\",\"estimated_minutes\":20,\"priority\":\"high\",\"status\":\"ready\",\"scheduled_date\":\"2026-09-06\",\"version\":1}]}"},
      {true, 200, "{\"last_acked_sequence\":0,\"server_time\":1700000001,\"results\":[]}"},
  };
  sync::LearningBackendSession session(
      app, std::move(http), "http://relay:8000",
      domain::DeviceId{"dev-1"}, domain::ChildId{"child-1"},
      [](const std::string& device, const std::string& challenge,
         const std::string& nonce) {
        return device + ":" + challenge + ":" + nonce;
      },
      [] { return int64_t{1700000001}; });

  CHECK(session.diagnostics().configured);
  CHECK(session.runOnlineCycle());
  // No local events exist in this cycle, so the coordinator returns
  // NoPending without issuing an events request: challenge + auth + today.
  CHECK(raw->requests == 3);
  CHECK(session.authenticated());
  CHECK(session.diagnostics().last_operation == "events");
  CHECK(session.diagnostics().last_error == sync::SyncErrorClass::None);
  CHECK(app.state().tasks.size() == 1);
  CHECK(app.state().tasks.front().child_id == domain::ChildId{"child-1"});
}

void RunUnconfiguredIsSafe() {
  auto disk = std::make_shared<sync::FakeDisk>();
  sync::FakeOutboxStorage storage(disk);
  domain::DomainReducer reducer;
  application::AppCoordinator app(storage, reducer);
  sync::LearningBackendSession session(
      app, nullptr, "", domain::DeviceId{"dev-1"},
      domain::ChildId{"child-1"}, {});
  CHECK(!session.diagnostics().configured);
  CHECK(!session.authenticate());
  CHECK(session.diagnostics().last_operation == "auth_not_configured");
  CHECK(session.diagnostics().pending_count == 0);
}

void RejectMalformedToday() {
  sync::wire::TodayResponse today;
  CHECK(!sync::wire::DecodeTodayResponse(
      "{\"date\":\"2026-09-06\",\"tasks\":[],\"extra\":" +
      std::string(20, '[') + "0" + std::string(20, ']') + "}", today));
  CHECK(!sync::wire::DecodeTodayResponse(
      "{\"date\":\"2026-09-06\",\"tasks\":[{\"task_id\":\"t\",\"title\":\"Math\","
      "\"subject\":\"math\",\"estimated_minutes\":4294967297,\"priority\":\"high\","
      "\"status\":\"ready\",\"scheduled_date\":\"2026-09-06\",\"version\":1}]}", today));
}

void RejectIncompleteAcknowledgement() {
  FakeHttp transport;
  transport.responses = {{true, 200,
      "{\"last_acked_sequence\":2,\"server_time\":1700000000,\"results\":["
      "{\"event_id\":\"e1\",\"sequence\":1,\"status\":\"accepted\",\"http_status\":200},"
      "{\"event_id\":\"e2\",\"sequence\":2}]}"}};
  sync::BackendClient client(transport, "https://relay");
  const auto result = client.syncBatch({});
  CHECK(result.error_class == sync::SyncErrorClass::Unknown);
  CHECK(result.batch.last_acked_sequence == 0 && result.batch.results.empty());
}

}  // namespace

int main() {
  RunConfiguredCycle();
  RunUnconfiguredIsSafe();
  RejectMalformedToday();
  RejectIncompleteAcknowledgement();
  if (g_fail == 0) {
    std::printf("learning_backend_session_tests: all PASS\n");
    return 0;
  }
  std::printf("learning_backend_session_tests: %d FAILURES\n", g_fail);
  return 1;
}
