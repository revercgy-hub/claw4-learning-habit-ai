// claw4/firmware/tests/unit/ui/presenter_tests.cpp
// Host unit tests for the four MVP screen presenters (WB-STREAM-002 CP4).
//
// The presenters are pure functions over read-only snapshots; the tests build
// ViewState fixtures directly and never go through LVGL/network/BSP. The
// domain state machine stays the single authority — tests assert the
// presenter only enables actions the domain allows and never fabricates
// intents (e.g. a segment timeout NEVER auto-completes anything).

#include <cstdint>
#include <cstdio>
#include <string>

#include "ui/presenters.h"
#include "ui/view_state.h"

using namespace claw4::domain;
using namespace claw4::ui;

static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                                     \
  do {                                                 \
    ++g_cases;                                         \
    if (!run_case_##name()) {                          \
      std::printf("FAIL: %s\n", #name);                \
      ++g_fail;                                        \
    }                                                  \
  } while (0)

#define CHECK(cond)                                                      \
  do {                                                                   \
    if (!(cond)) {                                                       \
      std::printf("  assert fail: %s (line %d)\n", #cond, __LINE__);     \
      return false;                                                      \
    }                                                                    \
  } while (0)

// ---------------------------------------------------------------------------
// fixtures
// ---------------------------------------------------------------------------
static Task makeTask(const char* id, const char* title, const char* subject,
                     int minutes, TaskStatus st, int64_t version = 1) {
  Task t;
  t.task_id = TaskId{id};
  t.title = title;
  t.subject = subject;
  t.estimated_minutes = minutes;
  t.status = st;
  t.version = version;
  return t;
}

static ViewState makeVS(DeviceState ds, std::vector<Task> tasks,
                        std::optional<StudySession> session,
                        int pending = 0, bool synced = true) {
  ViewState vs;
  vs.domain.device_state = ds;
  vs.domain.tasks = std::move(tasks);
  vs.domain.active_session = std::move(session);
  vs.domain.pending_event_count = pending;
  vs.domain.time_synced = synced;
  return vs;
}

static StudySession makeSession(const char* sid, const char* tid,
                                SessionStatus st, int planned_min,
                                int64_t actual_s, int64_t seg_start_ms,
                                int64_t paused_at_ms = 0) {
  StudySession s;
  s.session_id = SessionId{sid};
  s.task_id = TaskId{tid};
  s.planned_minutes = planned_min;
  s.actual_seconds = actual_s;
  s.status = st;
  s.segment_start_monotonic_ms = seg_start_ms;
  s.paused_at_monotonic_ms = paused_at_ms;
  return s;
}

// ---------------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------------
static bool run_case_home_ready_card_fields() {
  ViewState vs = makeVS(DeviceState::OnlineIdle,
                        {makeTask("t1", "Math drill", "math", 30, TaskStatus::Ready)}, {});
  HomeView v = buildHome(vs);
  CHECK(v.has_tasks);
  CHECK(v.cards.size() == 1);
  CHECK(v.cards[0].task_id == TaskId{"t1"});
  CHECK(v.cards[0].title == "Math drill");
  CHECK(v.cards[0].subject == "math");
  CHECK(v.cards[0].estimated_minutes == 30);
  CHECK(v.cards[0].start_enabled);
  CHECK(v.can_start);
  CHECK(!v.offline);
  CHECK(v.hint == PageHint::None);
  return true;
}

static bool run_case_home_empty_state() {
  ViewState vs = makeVS(DeviceState::OnlineIdle, {}, {});
  HomeView v = buildHome(vs);
  CHECK(!v.has_tasks);
  CHECK(v.cards.empty());
  CHECK(!v.can_start);
  CHECK(v.hint == PageHint::Empty);
  return true;
}

static bool run_case_home_pending_sync_count() {
  ViewState vs = makeVS(DeviceState::Syncing,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Paused)}, {},
                        5);
  HomeView v = buildHome(vs);
  CHECK(v.pending_sync_count == 5);
  return true;
}

static bool run_case_home_offline_badge() {
  ViewState vs = makeVS(DeviceState::OfflineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Ready)}, {});
  HomeView v = buildHome(vs);
  CHECK(v.offline);
  CHECK(v.hint == PageHint::Offline);
  return true;
}

static bool run_case_home_start_mapping() {
  ViewState vs = makeVS(DeviceState::OnlineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Ready)}, {});
  HomeView v = buildHome(vs);
  auto r = mapHomeStart(v.cards[0]);
  CHECK(r.has_value());
  CHECK(r->intent == Intent::StartTask);
  CHECK(r->task_id == TaskId{"t1"});
  return true;
}

static bool run_case_home_start_disabled_with_active_session() {
  ViewState vs = makeVS(DeviceState::Focusing,
                        {makeTask("t1", "A", "math", 10, TaskStatus::InProgress)},
                        makeSession("s1", "t1", SessionStatus::Running, 10, 5, 1000));
  HomeView v = buildHome(vs);
  CHECK(!v.can_start);
  CHECK(!v.cards[0].start_enabled);
  CHECK(v.cards[0].active);
  CHECK(mapHomeStart(v.cards[0]) == std::nullopt);
  return true;
}

static bool run_case_home_completed_card_not_startable() {
  ViewState vs = makeVS(DeviceState::OnlineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Completed)}, {});
  HomeView v = buildHome(vs);
  CHECK(!v.cards[0].start_enabled);
  CHECK(mapHomeStart(v.cards[0]) == std::nullopt);
  return true;
}

static bool run_case_home_skipped_card_not_startable() {
  ViewState vs = makeVS(DeviceState::OnlineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Skipped)}, {});
  HomeView v = buildHome(vs);
  CHECK(!v.cards[0].start_enabled);
  return true;
}

static bool run_case_home_active_card_highlight() {
  ViewState vs = makeVS(DeviceState::Focusing,
                        {makeTask("t1", "A", "math", 10, TaskStatus::InProgress),
                         makeTask("t2", "B", "chinese", 5, TaskStatus::Ready)},
                        makeSession("s1", "t1", SessionStatus::Running, 10, 5, 1000));
  HomeView v = buildHome(vs);
  CHECK(v.cards[0].active);
  CHECK(!v.cards[1].active);
  CHECK(!v.cards[0].start_enabled);  // active session blocks all starts
  CHECK(!v.cards[1].start_enabled);
  return true;
}

// ---------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------
static bool run_case_focus_running_fields() {
  ViewState vs = makeVS(DeviceState::Focusing,
                        {makeTask("t1", "Math drill", "math", 30, TaskStatus::InProgress)},
                        makeSession("s1", "t1", SessionStatus::Running, 30, 0, 10'000));
  FocusView v = buildFocus(vs, 20'000);  // 10 s into the segment
  CHECK(v.has_session);
  CHECK(v.task_id == TaskId{"t1"});
  CHECK(v.session_id == SessionId{"s1"});
  CHECK(v.title == "Math drill");
  CHECK(v.subject == "math");
  CHECK(v.planned_minutes == 30);
  CHECK(v.running);
  CHECK(v.elapsed_ms == 10'000);
  CHECK(v.remaining_ms == 30 * 60'000 - 10'000);
  CHECK(v.pause_enabled);
  CHECK(!v.resume_enabled);
  CHECK(v.complete_enabled);
  CHECK(!v.timeout_prompt);
  CHECK(v.hint == PageHint::None);
  return true;
}

static bool run_case_focus_remaining_time_math() {
  // 60 s committed + running segment from 10'000 to 61'000 -> 111 s elapsed.
  ViewState vs = makeVS(DeviceState::Focusing,
                        {makeTask("t1", "A", "math", 3, TaskStatus::InProgress)},
                        makeSession("s1", "t1", SessionStatus::Running, 3, 60, 10'000));
  FocusView v = buildFocus(vs, 61'000);
  CHECK(v.elapsed_ms == 111'000);
  CHECK(v.remaining_ms == 180'000 - 111'000);
  return true;
}

static bool run_case_focus_paused_view() {
  ViewState vs = makeVS(DeviceState::Paused,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Paused)},
                        makeSession("s1", "t1", SessionStatus::Paused, 10, 120, 0, 55'000));
  FocusView v = buildFocus(vs, 99'000);  // paused: wall clock must not count
  CHECK(!v.running);
  CHECK(v.resume_enabled);
  CHECK(!v.pause_enabled);
  CHECK(v.complete_enabled);
  CHECK(v.elapsed_ms == 120'000);  // no running segment added
  return true;
}

static bool run_case_focus_tap_mappings() {
  ViewState vs = makeVS(DeviceState::Focusing,
                        {makeTask("t1", "A", "math", 10, TaskStatus::InProgress)},
                        makeSession("s1", "t1", SessionStatus::Running, 10, 0, 1000));
  FocusView v = buildFocus(vs, 2000);
  auto p = mapFocusTap(v, FocusTap::Pause);
  CHECK(p.has_value() && p->intent == Intent::Pause && p->session_id == SessionId{"s1"});
  CHECK(p->task_id == TaskId{"t1"});  // reducer keys transitions on task_id (P14.1)
  CHECK(mapFocusTap(v, FocusTap::Resume) == std::nullopt);
  auto c = mapFocusTap(v, FocusTap::Complete);
  CHECK(c.has_value() && c->intent == Intent::Complete && c->task_id == TaskId{"t1"});
  return true;
}

static bool run_case_focus_resume_mapping_when_paused() {
  ViewState vs = makeVS(DeviceState::Paused,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Paused)},
                        makeSession("s1", "t1", SessionStatus::Paused, 10, 60, 0, 9000));
  FocusView v = buildFocus(vs, 9000);
  CHECK(!v.pause_enabled);
  auto r = mapFocusTap(v, FocusTap::Resume);
  CHECK(r.has_value() && r->intent == Intent::Resume && r->task_id == TaskId{"t1"});
  CHECK(mapFocusTap(v, FocusTap::Pause) == std::nullopt);
  return true;
}

static bool run_case_focus_complete_disabled_on_completed_task() {
  // After the Task reached Completed the domain would reject Complete; the
  // presenter must disable it (no duplicate-intent path from the UI).
  ViewState vs = makeVS(DeviceState::OnlineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Completed)},
                        std::nullopt);
  FocusView v = buildFocus(vs, 0);
  CHECK(!v.has_session);
  CHECK(v.hint == PageHint::Error);
  CHECK(mapFocusTap(v, FocusTap::Complete) == std::nullopt);
  CHECK(mapFocusTap(v, FocusTap::Pause) == std::nullopt);
  return true;
}

static bool run_case_focus_timeout_prompt_no_autocomplete() {
  // Segment hits 0 while running: only a decision prompt may appear. The
  // presenter is a pure mapper — building the view must NOT emit or auto-apply
  // any completion; the user still decides via an explicit Complete tap.
  ViewState vs = makeVS(DeviceState::Focusing,
                        {makeTask("t1", "A", "math", 1, TaskStatus::InProgress)},
                        makeSession("s1", "t1", SessionStatus::Running, 1, 0, 1'000));
  FocusView v = buildFocus(vs, 61'000);  // segment start 1 s + 60 s > 60 s planned
  CHECK(v.running);
  CHECK(v.remaining_ms == 0);
  CHECK(v.timeout_prompt);
  // Completion still requires an explicit user tap; nothing is auto-completed
  // and the session/task state in the snapshot is untouched by the presenter.
  auto c = mapFocusTap(v, FocusTap::Complete);
  CHECK(c.has_value());  // user MAY still choose to complete...
  CHECK(v.complete_enabled);
  // ...but a second tap through a REBUILT view after the domain cleared the
  // session yields no intent (duplicate-click guard via state-machine truth).
  ViewState after = makeVS(DeviceState::OnlineIdle,
                           {makeTask("t1", "A", "math", 1, TaskStatus::Paused)},
                           std::nullopt);
  FocusView v2 = buildFocus(after, 0);
  CHECK(mapFocusTap(v2, FocusTap::Complete) == std::nullopt);
  return true;
}

static bool run_case_focus_no_session_error() {
  ViewState vs = makeVS(DeviceState::OnlineIdle,
                        {makeTask("t1", "A", "math", 5, TaskStatus::Ready)}, {});
  FocusView v = buildFocus(vs, 0);
  CHECK(!v.has_session);
  CHECK(v.hint == PageHint::Error);
  return true;
}

static bool run_case_focus_clock_no_regression() {
  // now < segment start must not underflow / fabricate time.
  ViewState vs = makeVS(DeviceState::Focusing,
                        {makeTask("t1", "A", "math", 5, TaskStatus::InProgress)},
                        makeSession("s1", "t1", SessionStatus::Running, 5, 30, 10'000));
  FocusView v = buildFocus(vs, 4'000);  // clock went backwards
  CHECK(v.elapsed_ms == 30'000);        // only committed seconds count
  CHECK(v.remaining_ms == 5 * 60'000 - 30'000);
  return true;
}

// ---------------------------------------------------------------------------
// Done
// ---------------------------------------------------------------------------
static bool run_case_done_normal_xp() {
  DoneInput in;
  in.session_id = SessionId{"s1"};
  in.subject = "math";
  in.title = "Math drill";
  in.actual_seconds = 125;
  in.completion_type = "manual";
  DoneView v = buildDone(in);
  CHECK(v.has_result);
  CHECK(v.actual_seconds == 125);
  CHECK(v.xp == 2);  // 1 XP per full minute (120 s -> 2 XP), no rewards economy
  CHECK(v.hint == PageHint::None);
  return true;
}

static bool run_case_done_empty() {
  DoneView v = buildDone(DoneInput{});
  CHECK(!v.has_result);
  CHECK(v.hint == PageHint::Empty);
  return true;
}

static bool run_case_done_short_duration_zero_xp() {
  DoneInput in;
  in.session_id = SessionId{"s1"};
  in.actual_seconds = 30;
  in.completion_type = "aborted";
  DoneView v = buildDone(in);
  CHECK(v.has_result);
  CHECK(v.xp == 0);
  CHECK(v.actual_seconds == 30);
  return true;
}

// ---------------------------------------------------------------------------
// Offline
// ---------------------------------------------------------------------------
static bool run_case_offline_view() {
  ViewState vs = makeVS(DeviceState::OfflineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Ready),
                         makeTask("t2", "B", "chinese", 5, TaskStatus::Completed)},
                        {}, 7, /*synced=*/false);
  OfflineView v = buildOffline(vs, /*sync_auth_paused=*/true);
  CHECK(v.offline);
  CHECK(v.pending_sync_count == 7);
  CHECK(v.last_acked_sequence == 0);
  CHECK(v.tasks_available);
  CHECK(v.sync_auth_paused);
  CHECK(v.cards.size() == 2);
  CHECK(v.cards[0].start_enabled);   // cached task still learnable offline
  CHECK(!v.cards[1].start_enabled);  // completed stays terminal
  CHECK(v.hint == PageHint::Offline);
  return true;
}

static bool run_case_offline_no_auth_pause_flag() {
  ViewState vs = makeVS(DeviceState::OfflineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Ready)}, {},
                        3);
  OfflineView v = buildOffline(vs, false);
  CHECK(v.offline);
  CHECK(!v.sync_auth_paused);
  return true;
}

static bool run_case_offline_start_mapping() {
  ViewState vs = makeVS(DeviceState::OfflineIdle,
                        {makeTask("t1", "A", "math", 10, TaskStatus::Paused)}, {});
  OfflineView v = buildOffline(vs, false);
  auto r = mapOfflineStart(v.cards[0]);
  CHECK(r.has_value() && r->intent == Intent::StartTask);
  return true;
}

// ---------------------------------------------------------------------------
// robustness: rapid taps, page switches, out-of-order updates
// ---------------------------------------------------------------------------
static bool run_case_stale_view_second_tap_rejected() {
  // Simulates a duplicate Complete arriving after the domain already finished:
  // the UI must rebuild from the newest snapshot; a stale view cannot emit a
  // second intent because the rebuilt view disables the action.
  ViewState active = makeVS(DeviceState::Focusing,
                            {makeTask("t1", "A", "math", 5, TaskStatus::InProgress)},
                            makeSession("s1", "t1", SessionStatus::Running, 5, 0, 1000));
  FocusView v1 = buildFocus(active, 2000);
  CHECK(v1.complete_enabled);
  // The tap is applied; the domain commits and the snapshot is updated.
  ViewState done = makeVS(DeviceState::OnlineIdle,
                          {makeTask("t1", "A", "math", 5, TaskStatus::Completed)}, {});
  FocusView v2 = buildFocus(done, 0);
  CHECK(!v2.has_session);
  CHECK(mapFocusTap(v2, FocusTap::Complete) == std::nullopt);
  // A STALE view can still map a tap (the presenter cannot know the world has
  // moved on) — real duplicate-click rejection is owned by the domain, which
  // returns Idempotent for a repeated Complete (verified in CP1). The UI's
  // responsibility is to rebuild from the newest snapshot before every tap.
  CHECK(mapFocusTap(v1, FocusTap::Complete).has_value());
  return true;
}

static bool run_case_page_switch_consistent() {
  // Rapid screen switching = repeated pure builds from the same snapshot; all
  // views must agree and none may mutate the input snapshot.
  ViewState vs = makeVS(DeviceState::Paused,
                        {makeTask("t1", "A", "math", 5, TaskStatus::Paused)},
                        makeSession("s1", "t1", SessionStatus::Paused, 5, 30, 0, 9000));
  const ViewState copy = vs;  // snapshot value
  for (int i = 0; i < 3; ++i) {
    HomeView h = buildHome(vs);
    FocusView f = buildFocus(vs, 9000);
    OfflineView o = buildOffline(vs, false);
    CHECK(h.cards.size() == 1);
    CHECK(f.resume_enabled);
    CHECK(o.cards.size() == 1);
    CHECK(f.elapsed_ms == 30'000);
  }
  // The snapshot must be untouched by any presenter.
  CHECK(vs.domain.tasks.size() == copy.domain.tasks.size());
  CHECK(vs.domain.active_session.has_value() == copy.domain.active_session.has_value());
  CHECK(vs.domain.active_session->actual_seconds == 30);
  return true;
}

static bool run_case_out_of_order_updates_consistent() {
  // Two out-of-order snapshots (old paused, new running) map independently;
  // each rebuild reflects exactly its snapshot - no cross-talk, no duplicate
  // intent fabrication.
  ViewState paused = makeVS(DeviceState::Paused,
                            {makeTask("t1", "A", "math", 5, TaskStatus::Paused)},
                            makeSession("s1", "t1", SessionStatus::Paused, 5, 30, 0, 9000));
  ViewState running = makeVS(DeviceState::Focusing,
                             {makeTask("t1", "A", "math", 5, TaskStatus::InProgress)},
                             makeSession("s1", "t1", SessionStatus::Running, 5, 30, 10'000));
  FocusView fp = buildFocus(paused, 9'000);
  FocusView fr = buildFocus(running, 12'000);
  CHECK(fp.resume_enabled && !fp.running);
  CHECK(fr.running && !fr.resume_enabled);
  CHECK(fr.elapsed_ms == 32'000);
  auto rp = mapFocusTap(fp, FocusTap::Resume);
  auto rr = mapFocusTap(fr, FocusTap::Resume);
  CHECK(rp.has_value());
  CHECK(rr == std::nullopt);  // the newer running view forbids Resume
  return true;
}

static bool run_case_offline_active_session_blocks_other_starts() {
  ViewState vs = makeVS(DeviceState::OfflineIdle,
                        {makeTask("t1", "A", "math", 5, TaskStatus::InProgress),
                         makeTask("t2", "B", "chinese", 5, TaskStatus::Ready)},
                        makeSession("s1", "t1", SessionStatus::Running, 5, 0, 1000));
  OfflineView v = buildOffline(vs, false);
  CHECK(v.cards[0].active);
  CHECK(!v.cards[1].start_enabled);  // one session at a time
  CHECK(mapOfflineStart(v.cards[1]) == std::nullopt);
  return true;
}

// ---------------------------------------------------------------------------
int main() {
  CASE(home_ready_card_fields);
  CASE(home_empty_state);
  CASE(home_pending_sync_count);
  CASE(home_offline_badge);
  CASE(home_start_mapping);
  CASE(home_start_disabled_with_active_session);
  CASE(home_completed_card_not_startable);
  CASE(home_skipped_card_not_startable);
  CASE(home_active_card_highlight);
  CASE(focus_running_fields);
  CASE(focus_remaining_time_math);
  CASE(focus_paused_view);
  CASE(focus_tap_mappings);
  CASE(focus_resume_mapping_when_paused);
  CASE(focus_complete_disabled_on_completed_task);
  CASE(focus_timeout_prompt_no_autocomplete);
  CASE(focus_no_session_error);
  CASE(focus_clock_no_regression);
  CASE(done_normal_xp);
  CASE(done_empty);
  CASE(done_short_duration_zero_xp);
  CASE(offline_view);
  CASE(offline_no_auth_pause_flag);
  CASE(offline_start_mapping);
  CASE(stale_view_second_tap_rejected);
  CASE(page_switch_consistent);
  CASE(out_of_order_updates_consistent);
  CASE(offline_active_session_blocks_other_starts);
  std::printf("presenter_tests: cases=%d failures=%d\n", g_cases, g_fail);
  std::printf("[gate] ui presenter host suite complete\n");
  return g_fail == 0 ? 0 : 1;
}
