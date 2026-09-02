// claw4/firmware/main/ui/presenters.h
// Host-testable view-model presenters for the four MVP screens
// (Home / Focus / Done / Offline) plus intent mapping.
//
// Portable C++17. The presenters are PURE mappings: (read-only ViewState /
// explicit UI-runtime inputs, injected clock) -> page view-model. They never
// mutate domain state, never render pixels, and never touch LVGL / network /
// GPIO / ESP-IDF / FreeRTOS / BSP / sync / application headers.
// The domain state machine remains the single authority: a presenter only
// ENABLES an action when the domain allows it, and the action is emitted as
// an IntentRequest through IntentSink (ARCHITECTURE.md §3.5 / §8.1).
//
// Contract source: ARCHITECTURE.md §8.1 / WB-STREAM-002 CP4.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/intents.h"
#include "learning_domain/task.h"
#include "ui/view_state.h"

namespace claw4 {
namespace ui {

// ---------------------------------------------------------------------------
// Shared rendering hints (semantic, not pixel styling).
// ---------------------------------------------------------------------------
enum class PageHint : uint8_t {
  None = 0,
  Empty,    // no data (e.g. no tasks, no result)
  Offline,  // device is offline / time not synced
  Error,    // view cannot be derived from the snapshot (inconsistent state)
};

// ---------------------------------------------------------------------------
// Home: today's task cards, pending sync, offline badge. Only StartTask.
// ---------------------------------------------------------------------------
struct HomeTaskCard {
  claw4::domain::TaskId task_id;
  std::string subject;
  std::string title;
  int estimated_minutes = 0;
  claw4::domain::TaskStatus status = claw4::domain::TaskStatus::Pending;
  bool start_enabled = false;  // domain allows StartTask right now
  bool active = false;         // this card is the task of the active session
};

struct HomeView {
  std::vector<HomeTaskCard> cards;
  bool has_tasks = false;      // false => Empty hint
  int pending_sync_count = 0;  // events waiting for sync
  bool offline = false;        // offline badge
  bool can_start = false;      // at least one card can start
  PageHint hint = PageHint::None;
};

// Builds the Home view-model from a read-only snapshot.
HomeView buildHome(const ViewState& vs);

// Maps a tap on one card to a StartTask intent. Returns nullopt when the card
// is not start-enabled (the domain would reject it anyway — the presenter
// never emits intents the domain forbids).
std::optional<claw4::domain::IntentRequest> mapHomeStart(const HomeTaskCard& card);

// ---------------------------------------------------------------------------
// Focus: task name, remaining time, Running/Paused, Pause/Resume/Complete.
// A segment timeout ONLY surfaces a decision prompt; nothing is auto-completed.
// ---------------------------------------------------------------------------
enum class FocusTap : uint8_t {
  Pause = 0,
  Resume,
  Complete,
};

struct FocusView {
  bool has_session = false;  // false => nothing to focus on
  claw4::domain::TaskId task_id;
  claw4::domain::SessionId session_id;
  std::string subject;
  std::string title;
  int planned_minutes = 0;
  int64_t elapsed_ms = 0;    // accumulated focus time including current segment
  int64_t remaining_ms = 0;  // planned_ms - elapsed_ms, clamped >= 0
  bool running = false;      // true: Running segment; false: Paused
  bool pause_enabled = false;
  bool resume_enabled = false;
  bool complete_enabled = false;
  bool timeout_prompt = false;  // segment reached 0: user decision only
  PageHint hint = PageHint::None;
};

// now_monotonic_ms is injected so remaining-time math is deterministic.
FocusView buildFocus(const ViewState& vs, int64_t now_monotonic_ms);

// Maps a Focus tap to the matching intent, or nullopt when the action is
// disabled in the current view.
std::optional<claw4::domain::IntentRequest> mapFocusTap(const FocusView& v, FocusTap tap);

// ---------------------------------------------------------------------------
// Done: actual learning time and a simple +XP readout. Informational only —
// there are no intents on this screen.
// ---------------------------------------------------------------------------
// The Done page renders the most recently finished session. That summary is
// UI-runtime state owned by the app shell (the domain snapshot clears the
// session once it is committed), so it is passed in explicitly.
struct DoneInput {
  std::optional<claw4::domain::SessionId> session_id;
  std::string subject;
  std::string title;
  int64_t actual_seconds = 0;
  std::string completion_type;  // "normal" / "manual" / "auto_saved" / "aborted"
  bool task_completed = false;  // whether the Task itself reached Completed
};

struct DoneView {
  bool has_result = false;
  std::string subject;
  std::string title;
  int64_t actual_seconds = 0;
  int xp = 0;  // MVP linear rule: 1 XP per full focus minute (see presenter)
  PageHint hint = PageHint::None;
};

DoneView buildDone(const DoneInput& in);

// ---------------------------------------------------------------------------
// Offline: cached tasks remain learnable, pending count, recoverable errors.
// ---------------------------------------------------------------------------
struct OfflineView {
  bool offline = false;
  int pending_sync_count = 0;
  int64_t last_acked_sequence = 0;
  bool tasks_available = false;  // cached tasks can still be started
  std::vector<HomeTaskCard> cards;
  bool sync_auth_paused = false;  // sync suspended until a re-auth succeeds
  PageHint hint = PageHint::None;
};

// sync_auth_paused is an explicit UI-runtime fact provided by the app shell
// (a presenter must not infer auth state from the domain snapshot).
OfflineView buildOffline(const ViewState& vs, bool sync_auth_paused);

// Offline reuses the same card model and mapping as Home.
std::optional<claw4::domain::IntentRequest> mapOfflineStart(const HomeTaskCard& card);

}  // namespace ui
}  // namespace claw4
