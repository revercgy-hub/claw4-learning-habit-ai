// CODEX-APP-FIRST-001 AF1b — BackendClient transport/endpoint contract tests.
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "sync/backend_client.h"

namespace {

using claw4::domain::ChildId;
using claw4::domain::DeviceEvent;
using claw4::domain::DeviceId;
using claw4::domain::EventId;
using claw4::domain::EventType;
using claw4::sync::BackendClient;
using claw4::sync::HttpResponse;
using claw4::sync::HttpTransport;
using claw4::sync::SyncClient;
using claw4::sync::SyncErrorClass;
using claw4::sync::wire::AuthResponse;
using claw4::sync::wire::TodayResponse;

int g_fail = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    if (!(x)) {                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);         \
      ++g_fail;                                                         \
    }                                                                   \
  } while (0)

struct RecordedRequest {
  std::string method;
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
};

class FakeHttp final : public HttpTransport {
 public:
  std::vector<RecordedRequest> requests;
  std::vector<HttpResponse> responses;

  HttpResponse request(const std::string& method, const std::string& url,
                       const std::vector<std::pair<std::string, std::string>>& headers,
                       const std::string& body, int64_t) override {
    requests.push_back({method, url, headers, body});
    if (responses.empty()) return HttpResponse{};
    HttpResponse response = responses.front();
    responses.erase(responses.begin());
    return response;
  }
};

bool HasHeader(const RecordedRequest& request, const char* name,
               const char* value) {
  for (const auto& header : request.headers) {
    if (header.first == name && header.second == value) return true;
  }
  return false;
}

DeviceEvent Event() {
  DeviceEvent event;
  event.event_id = EventId{"ev-1"};
  event.device_id = DeviceId{"dev-1"};
  event.child_id = ChildId{"child-1"};
  event.sequence = 1;
  event.timestamp = 1700000000;
  event.type = EventType::TaskStarted;
  event.payload["task_id"] = "task-1";
  return event;
}

void RunOnlineFlow() {
  FakeHttp http;
  http.responses = {
      {true, 200, "{\"challenge_id\":\"ch-1\",\"nonce\":\"nonce-1\",\"expires_at\":1700000100}"},
      {true, 200, "{\"access_token\":\"token-1\",\"expires_in\":3600}"},
      {true, 200, "{\"date\":\"2026-09-05\",\"tasks\":[{\"task_id\":\"task-1\",\"title\":\"Math\",\"subject\":\"math\",\"estimated_minutes\":20,\"priority\":\"high\",\"status\":\"ready\",\"scheduled_date\":\"2026-09-05\",\"version\":1}]}"},
      {true, 200, "{\"last_acked_sequence\":1,\"server_time\":1700000001,\"results\":[{\"event_id\":\"ev-1\",\"sequence\":1,\"status\":\"accepted\",\"http_status\":200}]}"},
  };
  BackendClient client(http, "http://relay:8000/");
  AuthResponse auth;
  SyncErrorClass error = SyncErrorClass::Unknown;
  bool signer_called = false;
  CHECK(client.authenticate(
      "dev-1",
      [&signer_called](const std::string& device, const std::string& challenge,
                       const std::string& nonce) {
        signer_called = device == "dev-1" && challenge == "ch-1" &&
                        nonce == "nonce-1";
        return std::string("sig-1");
      },
      auth, error));
  CHECK(signer_called);
  CHECK(error == SyncErrorClass::None);
  CHECK(auth.access_token == "token-1");
  CHECK(client.accessToken() == "token-1");

  TodayResponse today;
  CHECK(client.fetchToday("child-1", today, error));
  CHECK(today.tasks.size() == 1);
  CHECK(today.tasks[0].task_id.value == "task-1");

  SyncClient::Request batch;
  batch.device_id = DeviceId{"dev-1"};
  batch.events.push_back(Event());
  const auto response = client.syncBatch(batch);
  CHECK(response.error_class == SyncErrorClass::None);
  CHECK(response.http_status == 200);
  CHECK(response.batch.last_acked_sequence == 1);
  CHECK(response.batch.results.size() == 1);
  CHECK(http.requests.size() == 4);
  CHECK(http.requests[0].method == "POST");
  CHECK(http.requests[0].url == "http://relay:8000/api/v1/devices/challenge");
  CHECK(http.requests[1].url == "http://relay:8000/api/v1/devices/auth");
  CHECK(http.requests[1].body.find("\\\"challenge_signature\\\":\\\"sig-1\\\"") ==
        std::string::npos);  // signature is ordinary JSON, not double-encoded
  CHECK(http.requests[1].body.find("\"challenge_signature\":\"sig-1\"") !=
        std::string::npos);
  CHECK(HasHeader(http.requests[2], "Authorization", "Bearer token-1"));
  CHECK(http.requests[2].method == "GET");
  CHECK(http.requests[3].method == "POST");
  CHECK(http.requests[3].body.find("\"event_id\":\"ev-1\"") !=
        std::string::npos);
}

void RunErrorClassification() {
  FakeHttp http;
  http.responses = {{false, 0, ""}};
  BackendClient client(http, "http://relay:8000");
  AuthResponse auth;
  SyncErrorClass error = SyncErrorClass::None;
  CHECK(!client.authenticate("dev-1", [](const std::string&, const std::string&,
                                         const std::string&) { return "sig"; },
                             auth, error));
  CHECK(error == SyncErrorClass::Network);

  FakeHttp unauthorized;
  unauthorized.responses = {{true, 401, "{\"detail\":\"unauthorized\"}"}};
  BackendClient auth_client(unauthorized, "http://relay:8000");
  TodayResponse today;
  CHECK(!auth_client.fetchToday("child-1", today, error));
  CHECK(error == SyncErrorClass::Auth);

  FakeHttp server;
  server.responses = {{true, 503, "temporarily unavailable"}};
  BackendClient server_client(server, "http://relay:8000");
  SyncClient::Request request;
  request.device_id = DeviceId{"dev-1"};
  const auto response = server_client.syncBatch(request);
  CHECK(response.error_class == SyncErrorClass::Server);
  CHECK(response.http_status == 503);
}

}  // namespace

int main() {
  RunOnlineFlow();
  RunErrorClassification();
  if (g_fail == 0) {
    std::printf("backend_client_tests: all PASS\n");
    return 0;
  }
  std::printf("backend_client_tests: %d FAILURES\n", g_fail);
  return 1;
}
