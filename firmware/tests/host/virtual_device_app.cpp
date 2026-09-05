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

struct Runtime {
  explicit Runtime(std::shared_ptr<claw4::sync::FakeDisk> shared_disk)
      : disk(std::move(shared_disk)), storage(disk), app(storage, clock) {
    app.setIdentity(DeviceId{"virtual-device"}, ChildId{"child-demo"});
    app.start();
  }

  std::shared_ptr<claw4::sync::FakeDisk> disk;
  claw4::ports::FakeClockPort clock;
  claw4::sync::FakeOutboxStorage storage;
  LearningApp app;
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

}  // namespace

int main() {
  auto disk = std::make_shared<claw4::sync::FakeDisk>();
  std::unique_ptr<Runtime> runtime;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::string verb;
    std::string argument;
    if (!ParseCommand(line, verb, argument)) continue;

    if (verb == "boot") {
      runtime = std::make_unique<Runtime>(disk);
      PrintState(*runtime, "boot");
    } else if (verb == "restart") {
      if (!runtime) {
        std::cout << "{\"op\":\"restart\",\"ok\":false,\"error\":\"not_booted\"}\n";
        continue;
      }
      runtime.reset();
      runtime = std::make_unique<Runtime>(disk);
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
