// claw4/firmware/main/mcp/learning_mcp_host.cpp
// Implementation of the 8 learning.* MCP tools (WB-LEARNING-V4 P15).
//
// MVP semantic rules (fixed by the task pack, mirrored in tests):
//   - today list = backend snapshot .tasks (server-served today cache);
//   - get_remaining_time reuses ui::buildFocus math (planned_ms - elapsed_ms,
//     elapsed includes the running segment);
//   - get_today_progress = completed / (completed+inprogress+paused+ready+
//     skipped); Pending (future-date, not schedulable today) excluded;
//     denominator 0 -> percent 0;
//   - request_complete_task returns a "pending confirmation" and NEVER emits
//     Intent::Complete (AI cannot complete directly).
#include "mcp/learning_mcp_host.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "learning_domain/task.h"
#include "ui/presenters.h"
#include "ui/view_state.h"

namespace claw4 {
namespace mcp {

namespace {

// --- JSON-lite helpers (MVP: minimal escaping; real JSON codec is the P17
//     device MCP adapter concern) -------------------------------------------
void appendJsonString(std::string& out, const std::string& value) {
  out.push_back('"');
  for (const char c : value) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(c);
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
      out += buf;
    } else {
      out.push_back(c);
    }
  }
  out.push_back('"');
}

void appendJsonKey(std::string& out, const std::string& key) {
  appendJsonString(out, key);
  out.push_back(':');
}

void appendJsonInt(std::string& out, int64_t value) {
  out += std::to_string(value);
}

const char* taskStatusName(domain::TaskStatus status) {
  switch (status) {
    case domain::TaskStatus::Pending:
      return "pending";
    case domain::TaskStatus::Ready:
      return "ready";
    case domain::TaskStatus::InProgress:
      return "in_progress";
    case domain::TaskStatus::Paused:
      return "paused";
    case domain::TaskStatus::Completed:
      return "completed";
    case domain::TaskStatus::Skipped:
      return "skipped";
  }
  return "unknown";
}

const char* sessionStatusName(domain::SessionStatus status) {
  switch (status) {
    case domain::SessionStatus::Created:
      return "created";
    case domain::SessionStatus::Running:
      return "running";
    case domain::SessionStatus::Paused:
      return "paused";
    case domain::SessionStatus::Completed:
      return "completed";
    case domain::SessionStatus::Aborted:
      return "aborted";
  }
  return "unknown";
}

const char* intentResultName(domain::IntentResult result) {
  switch (result) {
    case domain::IntentResult::Accepted:
      return "accepted";
    case domain::IntentResult::Idempotent:
      return "idempotent";
    case domain::IntentResult::RejectedInvalidState:
      return "rejected";
    case domain::IntentResult::PersistFailed:
      return "persist_failed";
  }
  return "unknown";
}

McpResponse ok(std::string payload) { return {true, std::move(payload), ""}; }

McpResponse err(std::string message) { return {false, "", std::move(message)}; }

}  // namespace

McpResponse LearningMcpHost::invoke(const McpRequest& request) {
  const std::string& tool = request.tool;
  if (tool == kToolGetTodayTasks) return getTodayTasks();
  if (tool == kToolGetCurrentTask) return getCurrentTask();
  if (tool == kToolGetRemainingTime) return getRemainingTime();
  if (tool == kToolGetTodayProgress) return getTodayProgress();
  if (tool == kToolStartTask)
    return dispatchMutation(interaction::CommandKind::StartTask, request.args);
  if (tool == kToolPauseTask)
    return dispatchMutation(interaction::CommandKind::PauseTask, request.args);
  if (tool == kToolResumeTask)
    return dispatchMutation(interaction::CommandKind::ResumeTask, request.args);
  if (tool == kToolRequestCompleteTask)
    return dispatchMutation(interaction::CommandKind::CompleteTask, request.args);
  return err("unknown_tool:" + tool);
}

McpResponse LearningMcpHost::getTodayTasks() const {
  const auto& tasks = backend_.snapshot().tasks;
  std::string out = R"({"count":)" + std::to_string(tasks.size()) + R"(,"tasks":[)";
  for (std::size_t i = 0; i < tasks.size(); ++i) {
    if (i != 0) out.push_back(',');
    const auto& t = tasks[i];
    out.push_back('{');
    appendJsonKey(out, "task_id");
    appendJsonString(out, t.task_id.value);
    out.push_back(',');
    appendJsonKey(out, "title");
    appendJsonString(out, t.title);
    out.push_back(',');
    appendJsonKey(out, "subject");
    appendJsonString(out, t.subject);
    out.push_back(',');
    appendJsonKey(out, "estimated_minutes");
    appendJsonInt(out, t.estimated_minutes);
    out.push_back(',');
    appendJsonKey(out, "status");
    appendJsonString(out, taskStatusName(t.status));
    out.push_back('}');
  }
  out += "]}";
  return ok(std::move(out));
}

McpResponse LearningMcpHost::getCurrentTask() const {
  const auto d = backend_.snapshot();
  if (!d.active_session.has_value()) return ok(R"({"active":false})");

  const auto& s = *d.active_session;
  std::string out = R"({"active":true)";
  out += R"(,"session_status":)";
  appendJsonString(out, sessionStatusName(s.status));
  out += R"(,"task_id":)";
  appendJsonString(out, s.task_id.value);
  for (const auto& t : d.tasks) {
    if (t.task_id == s.task_id) {
      out += R"(,"title":)";
      appendJsonString(out, t.title);
      out += R"(,"subject":)";
      appendJsonString(out, t.subject);
      out += R"(,"task_status":)";
      appendJsonString(out, taskStatusName(t.status));
      break;
    }
  }
  out.push_back('}');
  return ok(std::move(out));
}

McpResponse LearningMcpHost::getRemainingTime() const {
  claw4::ui::ViewState vs;
  vs.screen = claw4::ui::Screen::Focus;
  vs.domain = backend_.snapshot();
  const claw4::ui::FocusView fv = claw4::ui::buildFocus(vs, backend_.nowMonotonicMs());
  if (!fv.has_session) return ok(R"({"active":false})");

  std::string out = R"({"active":true,"remaining_ms":)";
  out += std::to_string(fv.remaining_ms);
  out += R"(,"elapsed_ms":)";
  out += std::to_string(fv.elapsed_ms);
  out += R"(,"running":)";
  out += fv.running ? "true" : "false";
  out.push_back('}');
  return ok(std::move(out));
}

McpResponse LearningMcpHost::getTodayProgress() const {
  const auto& tasks = backend_.snapshot().tasks;
  int completed = 0;
  int actionable = 0;
  for (const auto& t : tasks) {
    if (t.status == domain::TaskStatus::Completed) ++completed;
    // Pending == planned for a FUTURE local date (task.h); not part of today's
    // actionable set, so it is excluded from the denominator.
    if (t.status != domain::TaskStatus::Pending) ++actionable;
  }
  const int percent = actionable > 0 ? (completed * 100) / actionable : 0;
  std::string out = R"({"completed":)";
  out += std::to_string(completed);
  out += R"(,"total":)";
  out += std::to_string(actionable);
  out += R"(,"percent":)";
  out += std::to_string(percent);
  out.push_back('}');
  return ok(std::move(out));
}

McpResponse LearningMcpHost::dispatchMutation(
    interaction::CommandKind kind,
    const std::map<std::string, std::string>& args) {
  const auto d = backend_.snapshot();

  // Resolve the target task: explicit arg wins; otherwise the task owning the
  // running/paused session (pause/resume/complete semantics).
  std::string task_id;
  const auto it = args.find("task_id");
  if (it != args.end() && !it->second.empty()) {
    task_id = it->second;
  } else if (d.active_session.has_value()) {
    task_id = d.active_session->task_id.value;
  }
  if (task_id.empty()) return err("missing_arg:task_id");

  domain::TaskId target;
  target.value = task_id;
  bool found = false;
  for (const auto& t : d.tasks) {
    if (t.task_id == target) {
      found = true;
      break;
    }
  }
  if (!found) return err("task_not_found:" + task_id);

  interaction::CommandPayload payload;
  payload.kind = kind;
  payload.task_id = target;
  if (d.active_session.has_value()) payload.session_id = d.active_session->session_id;

  const interaction::DispatchResult dr = dispatcher_.dispatch(
      interaction::CommandSource::Mcp, payload);
  if (dr.status == interaction::DispatchStatus::NeedsUserConfirmation) {
    // request_complete_task path: confirmation is pending; the child must
    // confirm physically on the device. Nothing was emitted.
    return ok(R"({"confirmation":"pending"})");
  }
  if (dr.status != interaction::DispatchStatus::Emitted) {
    return err("unmapped:" + std::to_string(static_cast<int>(kind)));
  }
  return ok(std::string(R"({"intent_result":")") + intentResultName(dr.intent_result) +
            "\"}");
}

}  // namespace mcp
}  // namespace claw4
