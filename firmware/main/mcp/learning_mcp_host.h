// claw4/firmware/main/mcp/learning_mcp_host.h
// Learning MCP tool host (V4 §8, WB-LEARNING-V4 P15).
// Portable C++17. No LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / BSP /
// Metalio includes.
//
// Exposes the 8 learning.* tools to the AI/assistant side. Mock transport for
// this stage = host tests calling invoke() directly; the REAL Metalio MCP
// transport bridge (ParseMessage / AddTool / JSON codec) is a §10 / P17
// device-side task.
//
// Rules enforced here (see also interaction/dispatcher.h):
//   - mutations (start/pause/resume) enter the domain ONLY through the
//     interaction::CommandDispatcher with CommandSource::Mcp,
//   - request_complete_task NEVER completes directly: the dispatcher records a
//     pending confirmation that only a physical Touch confirm releases,
//   - the four query tools are read-only projections over the backend snapshot
//     (queries never mutate domain state).
#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "interaction/dispatcher.h"
#include "learning_domain/domain_state.h"

namespace claw4 {
namespace mcp {

// Read-only learning backend injected into the MCP host. Mutations never live
// here: they flow through interaction::CommandDispatcher so the single-funnel
// and AI-completion-gate rules hold on the device too. A host glue / device
// adapter implements this over AppCoordinator + platform ports.
class LearningBackend {
 public:
  virtual ~LearningBackend() = default;
  virtual claw4::domain::DomainState snapshot() const = 0;
  virtual int64_t nowMonotonicMs() const = 0;
};

struct McpRequest {
  std::string tool;  // fully qualified: "learning.start_task", ...
  std::map<std::string, std::string> args;
};

// MVP transport-agnostic response. `payload` is a JSON-lite string produced by
// the host; real JSON coding is the device MCP adapter's concern.
struct McpResponse {
  bool ok = false;
  std::string payload;  // JSON-lite (queries / accepted mutations)
  std::string error;    // non-empty when !ok
};

class LearningMcpHost {
 public:
  LearningMcpHost(interaction::CommandDispatcher& dispatcher,
                  LearningBackend& backend)
      : dispatcher_(dispatcher), backend_(backend) {}

  static constexpr const char* kToolGetTodayTasks = "learning.get_today_tasks";
  static constexpr const char* kToolGetCurrentTask = "learning.get_current_task";
  static constexpr const char* kToolGetRemainingTime = "learning.get_remaining_time";
  static constexpr const char* kToolGetTodayProgress = "learning.get_today_progress";
  static constexpr const char* kToolStartTask = "learning.start_task";
  static constexpr const char* kToolPauseTask = "learning.pause_task";
  static constexpr const char* kToolResumeTask = "learning.resume_task";
  static constexpr const char* kToolRequestCompleteTask = "learning.request_complete_task";

  McpResponse invoke(const McpRequest& request);

 private:
  McpResponse getTodayTasks() const;
  McpResponse getCurrentTask() const;
  McpResponse getRemainingTime() const;
  McpResponse getTodayProgress() const;
  // Resolves task_id from args (fallback: the running session's task), checks
  // existence in the snapshot, then dispatches through the single funnel.
  McpResponse dispatchMutation(interaction::CommandKind kind,
                               const std::map<std::string, std::string>& args);

  interaction::CommandDispatcher& dispatcher_;
  LearningBackend& backend_;
};

}  // namespace mcp
}  // namespace claw4
