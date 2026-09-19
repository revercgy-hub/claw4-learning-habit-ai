// CODEX-APP-FIRST-001 AF0 — deterministic Virtual Device App runner.
//
// This runner is intentionally thin: every mutation goes through the real
// LearningApp -> CommandDispatcher -> AppCoordinator -> Outbox pipeline. It
// only provides a JSONL command shell and a shared FakeDisk so a process
// restart can model a device reboot without inventing a second state machine.

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "fakes/fake_outbox_storage.h"
#include "fakes/fake_platform_ports.h"
#include "metalio_claw4/host_glue/learning_app.h"

namespace {

using claw4::domain::ChildId;
using claw4::domain::DeviceId;
using claw4::domain::Task;
using claw4::domain::TaskId;
using claw4::domain::TaskStatus;
using claw4::interaction::CommandKind;
using claw4::interaction::CommandPayload;
using claw4::interaction::CommandSource;
using claw4::metalio::LearningApp;
using claw4::application::SyncOutcome;

class RunnerTransport final : public claw4::application::SyncTransport {
 public:
  enum class Mode { Offline, Accept, Duplicate, LostThenDuplicate };

  Mode mode = Mode::Offline;
  int calls = 0;

  claw4::sync::SyncClient::Response send(
      const claw4::sync::SyncClient::Request& request) override {
    ++calls;
    if (mode == Mode::Offline ||
        (mode == Mode::LostThenDuplicate && calls == 1)) {
      claw4::sync::SyncClient::Response response;
      response.error_class = claw4::sync::SyncErrorClass::Network;
      return response;
    }
    claw4::sync::SyncClient::Response response;
    response.error_class = claw4::sync::SyncErrorClass::None;
    response.http_status = 200;
    response.batch.last_acked_sequence = request.last_acked_sequence;
    response.batch.server_time = 1700000100 + calls;
    const auto outcome = mode == Mode::Duplicate
                             ? claw4::sync::EventOutcome::Duplicate
                             : claw4::sync::EventOutcome::Accepted;
    for (const auto& event : request.events) {
      claw4::sync::PerEventResult result;
      result.event_id = event.event_id;
      result.sequence = event.sequence;
      result.outcome = outcome;
      result.http_status = 200;
      response.batch.results.push_back(result);
      response.batch.last_acked_sequence = event.sequence;
    }
    return response;
  }
};

std::string JsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 2);
  for (const char ch : value) {
    if (ch == '\\' || ch == '"') {
      out.push_back('\\');
      out.push_back(ch);
    } else if (ch == '\n') {
      out += "\\n";
    } else if (ch == '\r') {
      out += "\\r";
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

const char* TaskStatusName(const TaskStatus status) {
  switch (status) {
    case TaskStatus::Pending:
      return "pending";
    case TaskStatus::Ready:
      return "ready";
    case TaskStatus::InProgress:
      return "in_progress";
    case TaskStatus::Paused:
      return "paused";
    case TaskStatus::Completed:
      return "completed";
    case TaskStatus::Skipped:
      return "skipped";
  }
  return "unknown";
}

const char* CommandName(const CommandKind kind) {
  switch (kind) {
    case CommandKind::StartTask:
      return "start";
    case CommandKind::PauseTask:
      return "pause";
    case CommandKind::ResumeTask:
      return "resume";
    case CommandKind::CompleteTask:
      return "complete";
    default:
      return "unknown";
  }
}

const char* SyncOutcomeName(const SyncOutcome outcome) {
  switch (outcome) {
    case SyncOutcome::Synced: return "synced";
    case SyncOutcome::ReauthOk: return "reauth_ok";
    case SyncOutcome::PausedAuth: return "paused_auth";
    case SyncOutcome::Backoff: return "backoff";
    case SyncOutcome::NoPending: return "no_pending";
    case SyncOutcome::StaleResult: return "stale_result";
    case SyncOutcome::Blocked: return "blocked";
  }
  return "unknown";
}

struct Runtime {
  explicit Runtime(std::shared_ptr<claw4::sync::FakeDisk> shared_disk,
                   std::shared_ptr<int64_t> event_sequence,
                   std::shared_ptr<int64_t> session_sequence)
      : disk(std::move(shared_disk)), storage(disk), app(storage, clock) {
    app.setIdentity(DeviceId{"virtual-device"}, ChildId{"child-demo"});
    // A reboot must never reset persisted event/session identity. The real
    // device uses entropy-backed IDs; the deterministic runner keeps process-
    // lifetime counters shared by all Runtime instances in this scenario.
    app.setEventIdFactory([event_sequence]() {
      return claw4::domain::EventId{"ev-" +
                                    std::to_string(++(*event_sequence))};
    });
    app.setSessionIdFactory([session_sequence]() {
      return claw4::domain::SessionId{"sess-" +
                                      std::to_string(++(*session_sequence))};
    });
    app.start();
  }

  std::shared_ptr<claw4::sync::FakeDisk> disk;
  claw4::ports::FakeClockPort clock;
  claw4::sync::FakeOutboxStorage storage;
  LearningApp app;
  RunnerTransport transport;
};

void PrintState(const Runtime& runtime, const std::string& op,
                bool ok = true, const std::string& error = "") {
  const auto& state = runtime.app.state();
  std::cout << "{\"op\":\"" << JsonEscape(op) << "\",\"ok\":"
            << (ok ? "true" : "false")
            << ",\"pending\":" << runtime.app.pendingCount()
            << ",\"last_acked\":" << runtime.app.lastAcked()
            << ",\"active\":";
  if (state.active_session) {
    std::cout << "\"" << JsonEscape(state.active_session->task_id.value)
              << "\"";
  } else {
    std::cout << "null";
  }
  std::cout << ",\"tasks\":[";
  for (std::size_t i = 0; i < state.tasks.size(); ++i) {
    if (i != 0) std::cout << ',';
    const auto& task = state.tasks[i];
    std::cout << "{\"id\":\"" << JsonEscape(task.task_id.value)
              << "\",\"status\":\"" << TaskStatusName(task.status)
              << "\"}";
  }
  std::cout << "]";
  if (!error.empty()) {
    std::cout << ",\"error\":\"" << JsonEscape(error) << "\"";
  }
  std::cout << "}\n";
  std::cout.flush();
}

bool SeedTasks(Runtime& runtime) {
  Task math;
  math.task_id = TaskId{"demo-math-001"};
  math.child_id = ChildId{"child-demo"};
  math.title = "Fractions";
  math.subject = "math";
  math.description = "Practice fractions";
  math.task_type = "practice";
  math.estimated_minutes = 25;
  math.priority = "high";
  math.status = TaskStatus::Ready;
  math.scheduled_date = "2026-09-05";
  math.version = 1;

  Task reading = math;
  reading.task_id = TaskId{"demo-reading-001"};
  reading.title = "Reading notes";
  reading.subject = "chinese";
  reading.priority = "medium";

  return runtime.app.applyTodaySnapshot({math, reading});
}

bool ParseCommand(const std::string& line, std::string& verb,
                  std::string& argument) {
  std::istringstream input(line);
  if (!(input >> verb)) return false;
  input >> argument;
  return true;
}

bool DispatchTouch(Runtime& runtime, const std::string& action,
                   const std::string& task_id) {
  CommandKind kind = CommandKind::Unknown;
  if (action == "start") kind = CommandKind::StartTask;
  if (action == "pause") kind = CommandKind::PauseTask;
  if (action == "resume") kind = CommandKind::ResumeTask;
  if (action == "complete") kind = CommandKind::CompleteTask;
  if (kind == CommandKind::Unknown || task_id.empty()) return false;

  CommandPayload payload;
  payload.kind = kind;
  payload.task_id = TaskId{task_id};
  const auto result = runtime.app.dispatcher().dispatch(
      CommandSource::Touch, payload);
  const bool ok = result.status == claw4::interaction::DispatchStatus::Emitted &&
                  result.intent_result == claw4::domain::IntentResult::Accepted;
  PrintState(runtime, std::string("touch_") + CommandName(kind), ok,
             ok ? "" : "transition_not_accepted");
  return ok;
}

void RunSync(Runtime& runtime) {
  const auto outcome = runtime.app.runSyncOnce(runtime.transport, [] { return false; });
  PrintState(runtime, std::string("sync_") + SyncOutcomeName(outcome),
             outcome != SyncOutcome::PausedAuth,
             outcome == SyncOutcome::Backoff ? "network_unavailable" : "");
}

}  // namespace

int main() {
  auto disk = std::make_shared<claw4::sync::FakeDisk>();
  auto event_sequence = std::make_shared<int64_t>(0);
  auto session_sequence = std::make_shared<int64_t>(0);
  std::unique_ptr<Runtime> runtime;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::string verb;
    std::string argument;
    if (!ParseCommand(line, verb, argument)) continue;

    if (verb == "boot") {
      runtime = std::make_unique<Runtime>(disk, event_sequence, session_sequence);
      PrintState(*runtime, "boot");
    } else if (verb == "restart") {
      if (!runtime) {
        std::cout << "{\"op\":\"restart\",\"ok\":false,\"error\":\"not_booted\"}\n";
        continue;
      }
      runtime.reset();
      runtime = std::make_unique<Runtime>(disk, event_sequence, session_sequence);
      PrintState(*runtime, "restart");
    } else if (verb == "seed") {
      if (!runtime) {
        std::cout << "{\"op\":\"seed\",\"ok\":false,\"error\":\"not_booted\"}\n";
        continue;
      }
      const bool ok = SeedTasks(*runtime);
      PrintState(*runtime, "seed", ok, ok ? "" : "snapshot_commit_failed");
    } else if (verb == "touch") {
      if (!runtime) {
        std::cout << "{\"op\":\"touch\",\"ok\":false,\"error\":\"not_booted\"}\n";
        continue;
      }
      const auto separator = argument.find(':');
      if (separator == std::string::npos) {
        std::cout << "{\"op\":\"touch\",\"ok\":false,\"error\":\"use_touch_action:task_id\"}\n";
        continue;
      }
      DispatchTouch(*runtime, argument.substr(0, separator),
                    argument.substr(separator + 1));
    } else if (verb == "network") {
      if (!runtime) {
        std::cout << "{\"op\":\"network\",\"ok\":false,\"error\":\"not_booted\"}\n";
        continue;
      }
      if (argument == "offline") runtime->transport.mode = RunnerTransport::Mode::Offline;
      else if (argument == "accept") runtime->transport.mode = RunnerTransport::Mode::Accept;
      else if (argument == "duplicate") runtime->transport.mode = RunnerTransport::Mode::Duplicate;
      else if (argument == "lost") {
        runtime->transport.mode = RunnerTransport::Mode::LostThenDuplicate;
        runtime->transport.calls = 0;
      } else {
        PrintState(*runtime, "network", false, "use_network:offline|accept|duplicate|lost");
        continue;
      }
      PrintState(*runtime, "network_" + argument);
    } else if (verb == "sync") {
      if (!runtime) {
        std::cout << "{\"op\":\"sync\",\"ok\":false,\"error\":\"not_booted\"}\n";
        continue;
      }
      RunSync(*runtime);
    } else if (verb == "summary") {
      if (!runtime) {
        std::cout << "{\"op\":\"summary\",\"ok\":false,\"error\":\"not_booted\"}\n";
        continue;
      }
      PrintState(*runtime, "summary");
    } else if (verb == "quit") {
      break;
    } else {
      if (runtime) {
        PrintState(*runtime, verb, false, "unknown_command");
      } else {
        std::cout << "{\"op\":\"" << JsonEscape(verb)
                  << "\",\"ok\":false,\"error\":\"not_booted\"}\n";
      }
    }
  }
  return 0;
}
