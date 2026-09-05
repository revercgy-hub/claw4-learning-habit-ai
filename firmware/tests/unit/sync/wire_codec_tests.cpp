// CODEX-APP-FIRST-001 AF1 — Auth/Today/Events wire codec contract tests.
// Host-only (pure C++17); this deliberately exercises the same JSON bytes the
// later HTTP/TLS transport will move to the backend.
#include <cstdio>
#include <string>

#include "learning_domain/event.h"
#include "learning_domain/task.h"
#include "sync/wire_codec.h"

namespace {

using claw4::domain::ChildId;
using claw4::domain::DeviceEvent;
using claw4::domain::DeviceId;
using claw4::domain::EventId;
using claw4::domain::EventType;
using claw4::domain::TaskStatus;
using claw4::domain::TimestampSource;
using claw4::sync::EventOutcome;
using claw4::sync::SyncClient;
using claw4::sync::wire::AuthRequest;
using claw4::sync::wire::AuthResponse;
using claw4::sync::wire::ChallengeResponse;
using claw4::sync::wire::DecodeAuthResponse;
using claw4::sync::wire::DecodeBatchResponse;
using claw4::sync::wire::DecodeChallengeResponse;
using claw4::sync::wire::DecodeTodayResponse;
using claw4::sync::wire::EncodeAuthRequest;
using claw4::sync::wire::EncodeBatchRequest;
using claw4::sync::wire::EncodeChallengeRequest;
using claw4::sync::wire::EncodeTodayRequest;
using claw4::sync::wire::TodayResponse;

int g_fail = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    if (!(x)) {                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);         \
      ++g_fail;                                                         \
    }                                                                   \
  } while (0)

void RunAuth() {
  CHECK(EncodeChallengeRequest({"dev-1"}) == "{\"device_id\":\"dev-1\"}");
  ChallengeResponse challenge;
  CHECK(DecodeChallengeResponse(
      "{\"challenge_id\":\"ch-1\",\"nonce\":\"n-1\",\"expires_at\":1700000000}",
      challenge));
  CHECK(challenge.challenge_id == "ch-1");
  CHECK(challenge.nonce == "n-1");
  CHECK(challenge.expires_at == 1700000000);
  CHECK(!DecodeChallengeResponse("{\"challenge_id\":\"ch-1\"}", challenge));

  AuthRequest request;
  request.device_id = "claw4-\\\"dev";
  request.challenge_id = "challenge-口算";
  request.nonce = "n\\n1";
  request.challenge_signature = "sig/with\\slash";
  const std::string json = EncodeAuthRequest(request);
  CHECK(json ==
        "{\"device_id\":\"claw4-\\\\\\\"dev\",\"challenge_id\":\"challenge-口算\","
        "\"nonce\":\"n\\\\n1\",\"challenge_signature\":\"sig/with\\\\slash\"}");

  AuthResponse response;
  CHECK(DecodeAuthResponse("{\"access_token\":\"tok-1\",\"expires_in\":3600}",
                           response));
  CHECK(response.access_token == "tok-1");
  CHECK(response.expires_in == 3600);
  CHECK(!DecodeAuthResponse("{\"access_token\":\"tok\"}", response));
  CHECK(!DecodeAuthResponse("not-json", response));
}

void RunToday() {
  CHECK(EncodeTodayRequest({"child-1"}) == "{\"child_id\":\"child-1\"}");
  const std::string json =
      "{\"date\":\"2026-09-05\",\"tasks\":["
      "{\"task_id\":\"task-口算\",\"title\":\"口算\",\"subject\":\"math\","
      "\"estimated_minutes\":20,\"priority\":\"high\",\"status\":\"ready\","
      "\"scheduled_date\":\"2026-09-05\",\"version\":7},"
      "{\"task_id\":\"task-2\",\"title\":\"背诵\",\"subject\":\"chinese\","
      "\"estimated_minutes\":15,\"priority\":\"low\",\"status\":\"paused\","
      "\"scheduled_date\":\"2026-09-05\",\"version\":3}]}";
  TodayResponse response;
  CHECK(DecodeTodayResponse(json, response));
  CHECK(response.date == "2026-09-05");
  CHECK(response.tasks.size() == 2);
  CHECK(response.tasks[0].task_id.value == "task-口算");
  CHECK(response.tasks[0].status == TaskStatus::Ready);
  CHECK(response.tasks[0].version == 7);
  CHECK(response.tasks[1].status == TaskStatus::Paused);
  CHECK(!DecodeTodayResponse("{\"date\":\"2026-09-05\",\"tasks\":[{}]}", response));
  CHECK(!DecodeTodayResponse("{\"date\":\"2026-09-05\",\"tasks\":null}", response));
}

DeviceEvent MakeEvent(const char* id, int64_t sequence, EventType type) {
  DeviceEvent event;
  event.event_id = EventId{id};
  event.device_id = DeviceId{"dev-1"};
  event.child_id = ChildId{"child-1"};
  event.sequence = sequence;
  event.timestamp = 1700000000 + sequence;
  event.timestamp_source = TimestampSource::Rtc;
  event.type = type;
  event.version = 2;
  event.payload["task_id"] = "task-口算";
  event.payload["title"] = "带\"引号\"和换行\n";
  return event;
}

void RunBatch() {
  SyncClient::Request request;
  request.device_id = DeviceId{"dev-1"};
  request.last_acked_sequence = 4;
  request.events.push_back(MakeEvent("ev-1", 5, EventType::TaskStarted));
  request.events.push_back(MakeEvent("ev-2", 6, EventType::TaskCompleted));
  const std::string json = EncodeBatchRequest(request);
  CHECK(json.find("\"last_acked_sequence\":4") != std::string::npos);
  CHECK(json.find("\"type\":\"task.started\"") != std::string::npos);
  CHECK(json.find("\\\"引号\\\"") != std::string::npos);
  CHECK(json.find("\"timestamp_source\":\"rtc\"") != std::string::npos);
  CHECK(json.find("\"sequence\":6") != std::string::npos);

  SyncClient::Response response;
  const std::string reply =
      "{\"last_acked_sequence\":6,\"server_time\":1700000100,\"results\":["
      "{\"event_id\":\"ev-1\",\"sequence\":5,\"status\":\"accepted\",\"http_status\":200},"
      "{\"event_id\":\"ev-2\",\"sequence\":6,\"status\":\"duplicate\",\"http_status\":200}]}";
  CHECK(DecodeBatchResponse(reply, response));
  CHECK(response.error_class == claw4::sync::SyncErrorClass::None);
  CHECK(response.http_status == 200);
  CHECK(response.batch.last_acked_sequence == 6);
  CHECK(response.batch.server_time == 1700000100);
  CHECK(response.batch.results.size() == 2);
  CHECK(response.batch.results[0].outcome == EventOutcome::Accepted);
  CHECK(response.batch.results[1].outcome == EventOutcome::Duplicate);
  CHECK(response.batch.results[1].http_status == 200);
  CHECK(!DecodeBatchResponse("{\"last_acked_sequence\":6,\"server_time\":1,\"results\":["
                             "{\"event_id\":\"ev\",\"sequence\":7,\"status\":\"wat\","
                             "\"http_status\":400}]}", response));
}

}  // namespace

int main() {
  RunAuth();
  RunToday();
  RunBatch();
  if (g_fail == 0) {
    std::printf("wire_codec_tests: all PASS\n");
    return 0;
  }
  std::printf("wire_codec_tests: %d FAILURES\n", g_fail);
  return 1;
}
