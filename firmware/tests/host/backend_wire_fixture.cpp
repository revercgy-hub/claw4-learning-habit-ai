// CODEX-APP-FIRST-001 AF1c — C++ wire fixture used by the local Backend relay.
// This binary has no HTTP implementation: it emits/consumes the exact JSON
// owned by firmware/main/sync/wire_codec.cpp. Python is only the HTTP caller.
#include <cstdio>
#include <iostream>
#include <string>

#include "learning_domain/event.h"
#include "sync/wire_codec.h"

namespace {

using claw4::domain::ChildId;
using claw4::domain::DeviceEvent;
using claw4::domain::DeviceId;
using claw4::domain::EventId;
using claw4::domain::EventType;
using claw4::domain::TimestampSource;
using claw4::sync::SyncClient;
using claw4::sync::wire::AuthRequest;
using claw4::sync::wire::AuthResponse;
using claw4::sync::wire::ChallengeRequest;
using claw4::sync::wire::ChallengeResponse;
using claw4::sync::wire::TodayResponse;

EventType ParseEventType(const std::string& value) {
  if (value == "task.started") return EventType::TaskStarted;
  if (value == "task.completed") return EventType::TaskCompleted;
  if (value == "task.paused") return EventType::TaskPaused;
  if (value == "task.resumed") return EventType::TaskResumed;
  if (value == "study.session.started") return EventType::StudySessionStarted;
  if (value == "study.session.completed") return EventType::StudySessionCompleted;
  return EventType::DeviceBooted;
}

int Usage() {
  std::fprintf(stderr,
               "usage: backend_wire_fixture challenge|auth|batch|decode-today|decode-batch ...\n");
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return Usage();
  const std::string command = argv[1];
  if (command == "challenge" && argc == 3) {
    std::cout << claw4::sync::wire::EncodeChallengeRequest(
                     ChallengeRequest{argv[2]})
              << '\n';
    return 0;
  }
  if (command == "auth" && argc == 7) {
    AuthRequest request;
    request.device_id = argv[2];
    request.challenge_id = argv[3];
    request.nonce = argv[4];
    request.challenge_signature = argv[5];
    std::cout << claw4::sync::wire::EncodeAuthRequest(request) << '\n';
    return 0;
  }
  if (command == "batch" && argc >= 9) {
    SyncClient::Request request;
    request.device_id = DeviceId{argv[2]};
    request.last_acked_sequence = std::stoll(argv[3]);
    DeviceEvent event;
    event.event_id = EventId{argv[4]};
    event.device_id = DeviceId{argv[2]};
    event.child_id = ChildId{argv[5]};
    event.sequence = std::stoll(argv[6]);
    event.timestamp = std::stoll(argv[7]);
    event.timestamp_source = TimestampSource::Rtc;
    event.type = ParseEventType(argv[8]);
    event.version = 1;
    event.payload["task_id"] = argc >= 10 ? argv[9] : "task-1";
    if (event.type == EventType::StudySessionStarted ||
        event.type == EventType::StudySessionCompleted) {
      event.payload["session_id"] = argc >= 11 ? argv[10] : "session-1";
    }
    if (event.type == EventType::StudySessionCompleted) {
      event.payload["actual_seconds"] = "120";
      event.payload["completion_type"] = "manual";
    }
    request.events.push_back(event);
    std::cout << claw4::sync::wire::EncodeBatchRequest(request) << '\n';
    return 0;
  }
  if (command == "decode-today") {
    std::string json;
    std::getline(std::cin, json);
    TodayResponse response;
    if (!claw4::sync::wire::DecodeTodayResponse(json, response)) return 1;
    std::cout << "TODAY " << response.date << ' ' << response.tasks.size() << '\n';
    return 0;
  }
  if (command == "decode-batch") {
    std::string json;
    std::getline(std::cin, json);
    SyncClient::Response response;
    if (!claw4::sync::wire::DecodeBatchResponse(json, response)) return 1;
    std::cout << "ACK " << response.batch.last_acked_sequence << ' '
              << response.batch.results.size() << '\n';
    return 0;
  }
  return Usage();
}
