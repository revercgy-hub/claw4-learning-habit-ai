// claw4/firmware/main/learning_domain/domain_state.h
// Device-level and session-level state snapshots (read-only).
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §4.1.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "learning_domain/ids.h"
#include "learning_domain/intents.h"
#include "learning_domain/study_session.h"
#include "learning_domain/task.h"

namespace claw4 {
namespace domain {

// Device primary state (ARCHITECTURE.md §4.1). Maps onto the official
// device_state.h via a bridge; the learning state machine keeps its own copy.
enum class DeviceState : uint8_t {
  Booting = 0,
  Provisioning,
  Connecting,
  OnlineIdle,
  OfflineIdle,
  Focusing,
  Paused,
  Syncing,
  LowBattery,
  Error,
};

struct LearningCommandReceipt {
  static constexpr std::size_t kCapacity = 16;
  static constexpr int64_t kMaxLifetimeMs = 60'000;

  DeviceId device_id;
  ChildId child_id;
  int64_t session_epoch;
  std::string command_id;
  Intent intent;
  std::optional<TaskId> requested_task;
  std::optional<int64_t> expected_task_version;
  int64_t expires_at_ms;
  int64_t applied_task_version;
};

// Read-only domain snapshot consumed by the UI layer. The UI never mutates
// this; state transitions flow through intents -> domain services -> events
// (ARCHITECTURE.md §3.5 / I4).
struct DomainState {
  DeviceState device_state = DeviceState::Booting;
  // Device-side cache whose status changes participate in the same outbox
  // transition as the emitted business events.
  std::vector<Task> tasks;
  // Present when a session is active or was recovered after reboot.
  std::optional<StudySession> active_session;
  int pending_event_count = 0;  // events waiting for sync
  int64_t last_acked_sequence = 0;
  bool time_synced = false;
  // Suppress old requests even if a clock correction follows receipt pruning.
  int64_t learning_command_expired_through_ms = 0;
  std::vector<LearningCommandReceipt> learning_command_receipts;
};

}  // namespace domain
}  // namespace claw4
