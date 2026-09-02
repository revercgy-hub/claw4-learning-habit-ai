// claw4/firmware/main/learning_domain/reducer.h
// Pure deterministic domain reducer.
// Portable C++17. No LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / BSP / sync
// includes. Contract source: ARCHITECTURE.md §4 / WB-STREAM-002 CP1.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"
#include "learning_domain/intents.h"

namespace claw4 {
namespace domain {

// Every id, epoch time and monotonic time the reducer needs is injected by the
// caller. The reducer never touches the system clock, RNG or any I/O, so the
// same inputs always produce the same outputs and retries can reuse the same
// event_id values.
struct ReducerContext {
  DeviceId device_id;
  ChildId child_id;
  int64_t now_epoch = 0;          // unix epoch seconds, injected
  int64_t monotonic_ms = 0;       // injected monotonic clock
  // Deterministic id sources injected by the caller. A missing source makes
  // any transition that needs a fresh id fail (ok=false, no drafts).
  std::function<EventId()> make_event_id;    // injected
  std::function<SessionId()> make_session_id;  // injected
};

// Why an intent/recovery was rejected. `Idempotent` means the transition was
// already applied earlier (e.g. a duplicate Complete); nothing new is emitted.
enum class RejectReason : uint8_t {
  None = 0,
  TaskNotFound,
  TaskAlreadyStarted,
  TaskAlreadyCompleted,
  TaskAlreadySkipped,
  TaskNotInProgress,
  TaskNotPaused,
  SessionNotActive,
  SessionNotPaused,
  SkipNotAllowed,
  ClockWentBackwards,
  MissingIdSource,
  InvalidSnapshot,
};

struct TransitionResult {
  bool ok = false;
  RejectReason reason = RejectReason::None;
  // Accepted / Idempotent / RejectedInvalidState. Never PersistFailed here:
  // persistence is owned by the outbox (CP2).
  IntentResult intent_result = IntentResult::RejectedInvalidState;
  // Valid only when ok: the complete next domain snapshot.
  std::optional<DomainState> next;
  // Ordered event drafts (NO sequence — the outbox owns sequence allocation).
  // Empty whenever !ok, and empty for idempotent results.
  std::vector<EventDraft> drafts;
};

// Session recovery mode after a device reboot (ARCHITECTURE.md §4.4,
// CR-WB002-09): an intact snapshot resumes the SAME session for the user to
// decide; a corrupt snapshot can only produce `Aborted`. Neither ever
// auto-completes the Task.
enum class RecoveryMode : uint8_t {
  Intact = 0,
  Corrupt,
};

// Pure reducer: (immutable state, intent, context) -> next state + drafts.
// It never publishes UI, writes storage or sends network — the outbox
// (CP2) commits the transition atomically.
class DomainReducer {
 public:
  // Handles StartTask / Pause / Resume / Complete / Skip intents.
  TransitionResult reduce(const DomainState& state,
                          const IntentRequest& request,
                          const ReducerContext& ctx) const;

  // Focus-segment timeout: only pauses the segment for a user decision. It
  // NEVER completes the session or the task (ARCHITECTURE.md §4.4, I2).
  TransitionResult onSegmentTimeout(const DomainState& state,
                                    const SessionId& session_id,
                                    const ReducerContext& ctx) const;

  // Reboot recovery. Intact: resumes the same session (no state change, no
  // drafts — the caller shows the decision UI). Corrupt: marks the session
  // Aborted and never completes the Task.
  TransitionResult recoverSession(const DomainState& state,
                                  const SessionId& session_id,
                                  RecoveryMode mode,
                                  const ReducerContext& ctx) const;

  // After an intact-snapshot recovery the user explicitly chose to save/end:
  // session becomes Completed with CompletionType::AutoSaved; the Task is
  // NOT auto-completed (it returns to Paused so it can be started again).
  TransitionResult endRecoveredSession(const DomainState& state,
                                       const SessionId& session_id,
                                       const ReducerContext& ctx) const;

 private:
  struct Helpers;
};

}  // namespace domain
}  // namespace claw4
