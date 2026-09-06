// Host-only RPC shell. Python supplies HTTP, signing and atomic file I/O;
// all domain transitions, persistence encoding and wire JSON remain in C++.
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "fakes/fake_platform_ports.h"
#include "metalio_claw4/device/core/outbox_codec.h"
#include "metalio_claw4/host_glue/learning_app.h"
#include "sync/backend_sync_transport.h"

namespace {
using namespace claw4;

std::string Hex(const std::string& value) {
  if (value.empty()) return "-";
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (unsigned char c : value) {
    result += digits[c >> 4]; result += digits[c & 15];
  }
  return result;
}
std::string Unhex(const std::string& value) {
  if (value == "-") return {};
  if (value.size() % 2) throw std::runtime_error("invalid hex");
  std::string result;
  for (size_t i = 0; i < value.size(); i += 2) {
    size_t used = 0;
    auto byte = std::stoul(value.substr(i, 2), &used, 16);
    if (used != 2) throw std::runtime_error("invalid hex");
    result += static_cast<char>(byte);
  }
  return result;
}
std::string Rpc(const std::string& request) {
  std::cout << request << std::endl;
  std::string response;
  if (!std::getline(std::cin, response)) throw std::runtime_error("relay disconnected");
  return response;
}

class FileStorage final : public sync::OutboxStorage {
 public:
  bool load(sync::OutboxState& out) override {
    auto response = Rpc("LOAD");
    if (response == "MISSING") { out = {}; return true; }
    if (response == "ERROR") return false;
    return metalio::decodeOutboxState(Unhex(response), out);
  }
  sync::CommitStatus save(const sync::OutboxState& state) {
    std::string blob;
    if (!metalio::encodeOutboxState(state, blob)) return sync::CommitStatus::StorageError;
    return Rpc("SAVE " + Hex(blob)) == "OK" ? sync::CommitStatus::Committed
                                             : sync::CommitStatus::StorageError;
  }
  sync::CommitStatus commit(const domain::DomainState& domain,
                           const std::vector<sync::PendingEvent>& appended,
                           int64_t next) override {
    sync::OutboxState state;
    if (!load(state)) return sync::CommitStatus::StorageError;
    state.domain = domain;
    state.pending.insert(state.pending.end(), appended.begin(), appended.end());
    state.next_sequence = next;
    return save(state);
  }
  sync::CommitStatus commitDiagnostic(bool failed, bool recovered) override {
    sync::OutboxState state;
    if (!load(state)) return sync::CommitStatus::StorageError;
    state.diagnostic = {failed, recovered};
    return save(state);
  }
  sync::CommitStatus removeAcked(int64_t sequence) override {
    sync::OutboxState state;
    if (!load(state)) return sync::CommitStatus::StorageError;
    auto& pending = state.pending;
    pending.erase(std::remove_if(pending.begin(), pending.end(),
        [sequence](const auto& event) { return event.sequence <= sequence; }), pending.end());
    state.last_acked_sequence = std::max(state.last_acked_sequence, sequence);
    return save(state);
  }
  sync::CommitStatus markDeadLetter(const domain::EventId& id,
                                   const std::string& reason) override {
    sync::OutboxState state;
    if (!load(state)) return sync::CommitStatus::StorageError;
    for (auto& event : state.pending) if (event.event_id == id) event.dead_letter_reason = reason;
    return save(state);
  }
};

class RelayHttp final : public sync::HttpTransport {
 public:
  sync::HttpResponse request(const std::string& method, const std::string& url,
      const std::vector<std::pair<std::string, std::string>>& headers,
      const std::string& body, int64_t timeout) override {
    std::string authorization;
    for (const auto& h : headers) if (h.first == "Authorization") authorization = h.second;
    auto response = Rpc("HTTP " + method + " " + url + " " + Hex(authorization) +
                        " " + Hex(body) + " " + std::to_string(timeout));
    std::istringstream input(response);
    int status = 0;
    std::string encoded;
    if (!(input >> status >> encoded)) return {};
    return {status != 0, status, Unhex(encoded)};
  }
};
}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) return 2;  // base URL, device, child, boot UUID, epoch
  try {
    FileStorage storage;
    sync::OutboxState initial;
    if (!storage.load(initial)) throw std::runtime_error("storage_unreadable");
    ports::FakeClockPort clock;
    clock.epoch = std::stoll(argv[5]);
    metalio::LearningApp app(storage, clock);
    if (!app.coordinator().prepareAfterBoot(clock.monotonicMs()))
      throw std::runtime_error("boot_clock_commit_failed");
    app.setIdentity(domain::DeviceId{argv[2]}, domain::ChildId{argv[3]});
    int64_t ids = 0;
    const std::string boot = argv[4];
    app.setEventIdFactory([&] { return domain::EventId{boot + "-e" + std::to_string(++ids)}; });
    app.setSessionIdFactory([&] { return domain::SessionId{boot + "-s" + std::to_string(++ids)}; });
    app.start();
    RelayHttp http;
    sync::BackendClient client(http, argv[1]);
    sync::BackendSyncTransport transport(client);
    auto auth = [&] {
      sync::wire::AuthResponse response;
      sync::SyncErrorClass error;
      return client.authenticate(argv[2], [](const auto& device, const auto& challenge, const auto& nonce) {
        return Rpc("SIGN " + device + " " + challenge + " " + nonce);
      }, response, error);
    };
    auto state = [&](bool ok) {
      // Resolve RPC-backed fields BEFORE printing the result line.
      const int pending = app.pendingCount();
      const auto ack = app.lastAcked();
      std::cout << "RESULT {\"ok\":" << (ok ? "true" : "false")
                << ",\"pending\":" << pending << ",\"ack\":" << ack
                << ",\"active\":" << (app.state().active_session ? "true" : "false")
                << ",\"tasks\":[";
      bool first = true;
      for (const auto& task : app.state().tasks) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"id_hex\":\"" << Hex(task.task_id.value)
                  << "\",\"status\":" << static_cast<int>(task.status) << '}';
      }
      std::cout << "]}" << std::endl;
    };
    state(true);
    std::string line;
    while (std::getline(std::cin, line)) {
      std::istringstream input(line);
      std::string command, argument;
      input >> command >> argument;
      bool ok = true;
      if (command == "quit") break;
      if (command == "auth") ok = auth();
      else if (command == "today") {
        sync::wire::TodayResponse response;
        sync::SyncErrorClass error;
        ok = client.fetchToday(argv[3], response, error) && app.applyTodaySnapshot(response.tasks);
      } else if (command == "tick") {
        const auto seconds = std::stoll(argument);
        clock.epoch += seconds;
        clock.mono += seconds * 1000;
      }
      else if (command == "sync") {
        auto outcome = app.runSyncOnce(transport, auth);
        ok = outcome == application::SyncOutcome::Synced || outcome == application::SyncOutcome::NoPending;
      } else if (command != "state") {
        interaction::CommandPayload payload;
        if (command == "start") payload.kind = interaction::CommandKind::StartTask;
        else if (command == "pause") payload.kind = interaction::CommandKind::PauseTask;
        else if (command == "resume") payload.kind = interaction::CommandKind::ResumeTask;
        else if (command == "complete") payload.kind = interaction::CommandKind::CompleteTask;
        else throw std::runtime_error("unknown_command");
        payload.task_id = domain::TaskId{argument};
        const auto result = app.dispatcher().dispatch(interaction::CommandSource::Touch, payload);
        ok = result.status == interaction::DispatchStatus::Emitted &&
             result.intent_result == domain::IntentResult::Accepted;
      }
      state(ok);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
