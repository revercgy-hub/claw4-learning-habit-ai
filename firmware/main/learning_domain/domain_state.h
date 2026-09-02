// claw4/firmware/main/learning_domain/domain_state.h
// Device-level and session-level state snapshots (read-only).
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §4.1.
#pragma once

#include <cstdint>
#include <optional>

#include "learning_domain/ids.h"
#include "learning_domain/study_session.h"

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

// Read-only domain snapshot consumed by the UI layer. The UI never mutates
// this; state transitions flow through intents -> domain services -> events
// (ARCHITECTURE.md §3.5 / I4).
struct DomainState {
  DeviceState device_state = DeviceState::Booting;
  // Present when a session is active or was recovered after reboot.
  std::optional<StudySession> active_session;
  int pending_event_count = 0;  // events waiting for sync
  int64_t last_acked_sequence = 0;
  bool time_synced = false;
};

}  // namespace domain
}  // namespace claw4
