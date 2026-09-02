// claw4/firmware/main/ui/presenters.cpp
// Pure view-model presenters for Home / Focus / Done / Offline.
// No LVGL, no network, no GPIO/ESP-IDF/BSP, no sync/application headers.
// Contract source: ARCHITECTURE.md §8.1 / WB-STREAM-002 CP4.
#include "ui/presenters.h"

namespace claw4 {
namespace ui {

namespace {

bool isTerminal(const claw4::domain::TaskStatus s) {
  return s == claw4::domain::TaskStatus::Completed ||
         s == claw4::domain::TaskStatus::Skipped;
}

bool canStartStatus(const claw4::domain::TaskStatus s) {
  // Mirror the reducer's StartTask allow-set exactly: only Completed/Skipped/
  // InProgress are blocked (InProgress implies an active session anyway).
  return s != claw4::domain::TaskStatus::Completed &&
         s != claw4::domain::TaskStatus::Skipped &&
         s != claw4::domain::TaskStatus::InProgress;
}

bool isOffline(const claw4::domain::DomainState& d) {
  return d.device_state == claw4::domain::DeviceState::OfflineIdle ||
         d.device_state == claw4::domain::DeviceState::Error ||
         !d.time_synced;
}

}  // namespace

// ---------------------------------------------------------------------------
HomeView buildHome(const ViewState& vs) {
  HomeView out;
  const auto& d = vs.domain;
  const bool has_active = d.active_session.has_value();
  out.pending_sync_count = d.pending_event_count;
  out.offline = isOffline(d);

  for (const auto& t : d.tasks) {
    HomeTaskCard c;
    c.task_id = t.task_id;
    c.subject = t.subject;
    c.title = t.title;
    c.estimated_minutes = t.estimated_minutes;
    c.status = t.status;
    if (has_active && d.active_session->task_id == t.task_id) {
      c.active = true;
    }
    c.start_enabled = !has_active && canStartStatus(t.status);
    out.cards.push_back(std::move(c));
    if (c.start_enabled) out.can_start = true;
  }
  out.has_tasks = !out.cards.empty();
  if (!out.has_tasks) out.hint = PageHint::Empty;
  else if (out.offline) out.hint = PageHint::Offline;
  return out;
}

std::optional<claw4::domain::IntentRequest> mapHomeStart(const HomeTaskCard& card) {
  if (!card.start_enabled) return std::nullopt;
  claw4::domain::IntentRequest r;
  r.intent = claw4::domain::Intent::StartTask;
  r.task_id = card.task_id;
  return r;
}

// ---------------------------------------------------------------------------
FocusView buildFocus(const ViewState& vs, const int64_t now_monotonic_ms) {
  FocusView out;
  const auto& d = vs.domain;
  if (!d.active_session.has_value()) {
    out.hint = PageHint::Error;  // Focus screen with no session: inconsistent
    return out;
  }
  const auto& s = *d.active_session;
  const int ti = [&] {
    for (int i = 0; i < static_cast<int>(d.tasks.size()); ++i) {
      if (d.tasks[static_cast<std::size_t>(i)].task_id == s.task_id) return i;
    }
    return -1;
  }();
  out.has_session = true;
  out.session_id = s.session_id;
  out.task_id = s.task_id;
  out.planned_minutes = s.planned_minutes > 0
                            ? s.planned_minutes
                            : (ti >= 0 ? d.tasks[static_cast<std::size_t>(ti)].estimated_minutes
                                       : 0);
  if (ti >= 0) {
    out.subject = d.tasks[static_cast<std::size_t>(ti)].subject;
    out.title = d.tasks[static_cast<std::size_t>(ti)].title;
  }
  out.running = s.status == claw4::domain::SessionStatus::Running;
  const bool paused = s.status == claw4::domain::SessionStatus::Paused;
  const bool active = out.running || paused;

  // Elapsed = committed actual_seconds + the current running segment.
  int64_t seg_ms = 0;
  if (out.running && s.segment_start_monotonic_ms > 0) {
    if (now_monotonic_ms >= s.segment_start_monotonic_ms) {
      seg_ms = now_monotonic_ms - s.segment_start_monotonic_ms;
    }
  }
  out.elapsed_ms = s.actual_seconds * 1000 + seg_ms;
  const int64_t planned_ms = static_cast<int64_t>(out.planned_minutes) * 60'000;
  out.remaining_ms = (planned_ms - out.elapsed_ms) > 0 ? (planned_ms - out.elapsed_ms) : 0;

  if (active) {
    const auto tstat = ti >= 0 ? d.tasks[static_cast<std::size_t>(ti)].status
                               : claw4::domain::TaskStatus::InProgress;
    out.pause_enabled = out.running;
    out.resume_enabled = paused;
    // Complete mirrors the reducer: needs an active session whose task is
    // InProgress or Paused.
    out.complete_enabled =
        tstat == claw4::domain::TaskStatus::InProgress ||
        tstat == claw4::domain::TaskStatus::Paused;
    if (out.running && out.remaining_ms <= 0) {
      out.timeout_prompt = true;  // segment hit zero: decision prompt ONLY
    }
  } else {
    out.hint = PageHint::Error;
  }
  return out;
}

std::optional<claw4::domain::IntentRequest> mapFocusTap(const FocusView& v,
                                                        const FocusTap tap) {
  claw4::domain::IntentRequest r;
  r.session_id = v.session_id;
  switch (tap) {
    case FocusTap::Pause:
      if (!v.pause_enabled) return std::nullopt;
      r.intent = claw4::domain::Intent::Pause;
      return r;
    case FocusTap::Resume:
      if (!v.resume_enabled) return std::nullopt;
      r.intent = claw4::domain::Intent::Resume;
      return r;
    case FocusTap::Complete:
      if (!v.complete_enabled) return std::nullopt;
      r.intent = claw4::domain::Intent::Complete;
      return r;
  }
  return std::nullopt;
}

// ---------------------------------------------------------------------------
DoneView buildDone(const DoneInput& in) {
  DoneView out;
  if (!in.session_id.has_value()) {
    out.hint = PageHint::Empty;
    return out;
  }
  out.has_result = true;
  out.subject = in.subject;
  out.title = in.title;
  out.actual_seconds = in.actual_seconds > 0 ? in.actual_seconds : 0;
  // MVP linear +XP: 1 XP per full committed focus minute. This is NOT a
  // rewards economy — no store/ranks/animations (ARCHITECTURE.md §12 P2).
  out.xp = static_cast<int>(out.actual_seconds / 60);
  return out;
}

// ---------------------------------------------------------------------------
OfflineView buildOffline(const ViewState& vs, const bool sync_auth_paused) {
  OfflineView out;
  const auto& d = vs.domain;
  out.offline = isOffline(d);
  out.pending_sync_count = d.pending_event_count;
  out.last_acked_sequence = d.last_acked_sequence;
  out.sync_auth_paused = sync_auth_paused;

  const bool has_active = d.active_session.has_value();
  for (const auto& t : d.tasks) {
    HomeTaskCard c;
    c.task_id = t.task_id;
    c.subject = t.subject;
    c.title = t.title;
    c.estimated_minutes = t.estimated_minutes;
    c.status = t.status;
    if (has_active && d.active_session->task_id == t.task_id) c.active = true;
    // Offline cached tasks remain learnable: same allow-set as Home but not
    // gated on connectivity.
    c.start_enabled = !has_active && canStartStatus(t.status);
    out.cards.push_back(std::move(c));
    if (!isTerminal(t.status)) out.tasks_available = true;
  }
  if (out.offline) out.hint = PageHint::Offline;
  else if (out.cards.empty()) out.hint = PageHint::Empty;
  return out;
}

std::optional<claw4::domain::IntentRequest> mapOfflineStart(const HomeTaskCard& card) {
  return mapHomeStart(card);
}

}  // namespace ui
}  // namespace claw4
