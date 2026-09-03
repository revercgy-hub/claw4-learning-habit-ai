// claw4/firmware/tests/unit/interaction/dispatcher_tests.cpp
// Host unit tests for the V4 §7 interaction dispatcher + STT mapper
// (WB-LEARNING-V4 P14).
//
// Compiles, links and RUNS natively on Windows. Explicit case/assert counter
// (not bare `assert`) and non-zero exit on failure. Deterministic: the sink is
// a fake and no clock/RNG is involved.

#include <cstdint>
#include <cstdio>
#include <string>

#include "interaction/command.h"
#include "interaction/dispatcher.h"
#include "interaction/stt_mapper.h"
#include "learning_domain/intents.h"
#include "fakes/fake_command_sink.h"

using namespace claw4::domain;
using namespace claw4::interaction;

// ---------------------------------------------------------------------------
// tiny test harness
// ---------------------------------------------------------------------------
static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                                   \
  do {                                               \
    ++g_cases;                                       \
    if (!run_case_##name()) {                        \
      std::printf("FAIL: %s\n", #name);              \
      ++g_fail;                                      \
    }                                                \
  } while (0)

#define CHECK(cond)                                  \
  do {                                               \
    if (!(cond)) {                                   \
      std::printf("  assert fail: %s (line %d)\n",   \
                  #cond, __LINE__);                  \
      return false;                                  \
    }                                                \
  } while (0)

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
TaskId tid(const char* v) {
  TaskId t;
  t.value = v;
  return t;
}

CommandPayload payload(CommandKind kind, const char* task = nullptr,
                       const char* session = nullptr) {
  CommandPayload p;
  p.kind = kind;
  if (task) p.task_id = tid(task);
  if (session) {
    SessionId s;
    s.value = session;
    p.session_id = s;
  }
  return p;
}

// ---------------------------------------------------------------------------
// dispatcher cases
// ---------------------------------------------------------------------------
static bool run_case_touch_start_maps_intent_and_id() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  const auto r = d.dispatch(CommandSource::Touch,
                            payload(CommandKind::StartTask, "t-1"));
  CHECK(r.status == DispatchStatus::Emitted);
  CHECK(sink.emit_count() == 1);
  CHECK(sink.emitted().front().intent == Intent::StartTask);
  CHECK(sink.emitted().front().task_id.has_value());
  CHECK(sink.emitted().front().task_id->value == "t-1");
  return true;
}

static bool run_case_touch_pause_resume_skip_mappings() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);

  d.dispatch(CommandSource::Touch, payload(CommandKind::PauseTask, "t-1", "s-9"));
  CHECK(sink.emitted().back().intent == Intent::Pause);
  CHECK(sink.emitted().back().session_id.has_value());
  CHECK(sink.emitted().back().session_id->value == "s-9");

  d.dispatch(CommandSource::Touch, payload(CommandKind::ResumeTask, "t-1", "s-9"));
  CHECK(sink.emitted().back().intent == Intent::Resume);

  d.dispatch(CommandSource::Touch, payload(CommandKind::SkipTask, "t-1"));
  CHECK(sink.emitted().back().intent == Intent::Skip);
  CHECK(sink.emit_count() == 3);
  return true;
}

static bool run_case_touch_complete_emits_directly() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  const auto r = d.dispatch(CommandSource::Touch,
                            payload(CommandKind::CompleteTask, "t-1"));
  CHECK(r.status == DispatchStatus::Emitted);
  CHECK(sink.emit_count() == 1);
  CHECK(sink.emitted().front().intent == Intent::Complete);
  CHECK(sink.emitted().front().task_id->value == "t-1");
  CHECK(!d.hasPendingComplete());
  return true;
}

static bool run_case_voice_complete_requires_confirmation_no_emit() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  const auto r = d.dispatch(CommandSource::Voice,
                            payload(CommandKind::CompleteTask, "t-1"));
  CHECK(r.status == DispatchStatus::NeedsUserConfirmation);
  CHECK(sink.emit_count() == 0);  // never bypasses the gate
  CHECK(d.hasPendingComplete());
  return true;
}

static bool run_case_mcp_complete_requires_confirmation_no_emit() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  const auto r = d.dispatch(CommandSource::Mcp,
                            payload(CommandKind::CompleteTask, "t-1"));
  CHECK(r.status == DispatchStatus::NeedsUserConfirmation);
  CHECK(sink.emit_count() == 0);
  CHECK(d.hasPendingComplete());
  return true;
}

static bool run_case_repeated_mcp_complete_does_not_duplicate() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  d.dispatch(CommandSource::Mcp, payload(CommandKind::CompleteTask, "t-1"));
  d.dispatch(CommandSource::Mcp, payload(CommandKind::CompleteTask, "t-1"));
  CHECK(sink.emit_count() == 0);
  CHECK(d.hasPendingComplete());
  // one confirmation releases exactly one Complete
  const auto r = d.confirmPendingComplete();
  CHECK(r.status == DispatchStatus::Emitted);
  CHECK(sink.emit_count() == 1);
  CHECK(sink.emitted().front().intent == Intent::Complete);
  CHECK(!d.hasPendingComplete());
  return true;
}

static bool run_case_confirm_without_pending_rejected() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  const auto r = d.confirmPendingComplete();
  CHECK(r.status == DispatchStatus::NoPendingConfirmation);
  CHECK(sink.emit_count() == 0);
  return true;
}

static bool run_case_cancel_clears_pending() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  d.dispatch(CommandSource::Mcp, payload(CommandKind::CompleteTask, "t-1"));
  CHECK(d.hasPendingComplete());
  d.cancelPendingComplete();
  CHECK(!d.hasPendingComplete());
  const auto r = d.confirmPendingComplete();
  CHECK(r.status == DispatchStatus::NoPendingConfirmation);
  CHECK(sink.emit_count() == 0);
  return true;
}

static bool run_case_query_kinds_never_mutate() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  const CommandKind queries[] = {
      CommandKind::QueryTodayTasks, CommandKind::QueryCurrentTask,
      CommandKind::QueryRemainingTime, CommandKind::QueryTodayProgress};
  for (const auto k : queries) {
    const auto r = d.dispatch(CommandSource::Touch, payload(k));
    CHECK(r.status == DispatchStatus::UnsupportedKind);
    const auto rv = d.dispatch(CommandSource::Voice, payload(k));
    CHECK(rv.status == DispatchStatus::UnsupportedKind);
    const auto rm = d.dispatch(CommandSource::Mcp, payload(k));
    CHECK(rm.status == DispatchStatus::UnsupportedKind);
  }
  CHECK(sink.emit_count() == 0);
  return true;
}

static bool run_case_unknown_kind_rejected() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  const auto r = d.dispatch(CommandSource::Mcp, payload(CommandKind::Unknown));
  CHECK(r.status == DispatchStatus::UnsupportedKind);
  CHECK(sink.emit_count() == 0);
  return true;
}

static bool run_case_touch_complete_consumes_pending_ai_request() {
  FakeCommandSink sink;
  CommandDispatcher d(sink);
  d.dispatch(CommandSource::Mcp, payload(CommandKind::CompleteTask, "t-1"));
  CHECK(d.hasPendingComplete());
  // child physically completes the task: explicit action consumes the pending
  // AI request and emits exactly one Complete.
  const auto r = d.dispatch(CommandSource::Touch,
                            payload(CommandKind::CompleteTask, "t-1"));
  CHECK(r.status == DispatchStatus::Emitted);
  CHECK(sink.emit_count() == 1);
  CHECK(!d.hasPendingComplete());
  // a later confirm must find nothing pending
  const auto rc = d.confirmPendingComplete();
  CHECK(rc.status == DispatchStatus::NoPendingConfirmation);
  CHECK(sink.emit_count() == 1);
  return true;
}

// ---------------------------------------------------------------------------
// STT mapper cases
// ---------------------------------------------------------------------------
static bool run_case_stt_exact_phrases() {
  CHECK(mapVoicePhrase("开始学习").kind == CommandKind::StartTask);
  CHECK(mapVoicePhrase(" 开始任务 ").kind == CommandKind::StartTask);
  CHECK(mapVoicePhrase("暂停").kind == CommandKind::PauseTask);
  CHECK(mapVoicePhrase("继续学习").kind == CommandKind::ResumeTask);
  CHECK(mapVoicePhrase("恢复").kind == CommandKind::ResumeTask);
  CHECK(mapVoicePhrase("结束任务").kind == CommandKind::CompleteTask);
  CHECK(mapVoicePhrase("跳过").kind == CommandKind::SkipTask);
  CHECK(mapVoicePhrase("今天有什么任务").kind == CommandKind::QueryTodayTasks);
  CHECK(mapVoicePhrase("在学什么").kind == CommandKind::QueryCurrentTask);
  CHECK(mapVoicePhrase("还剩多久").kind == CommandKind::QueryRemainingTime);
  CHECK(mapVoicePhrase("今天进度").kind == CommandKind::QueryTodayProgress);
  return true;
}

static bool run_case_stt_complete_not_hijacked_by_query_keyword() {
  // "完成任务" must route to CompleteTask even though it contains 任务.
  const auto m = mapVoicePhrase("完成任务");
  CHECK(m.recognized);
  CHECK(m.kind == CommandKind::CompleteTask);
  return true;
}

static bool run_case_stt_keyword_fallback() {
  CHECK(mapVoicePhrase("帮我把学习暂停一下").kind == CommandKind::PauseTask);
  CHECK(mapVoicePhrase("帮我看看今天的任务").kind == CommandKind::QueryTodayTasks);
  CHECK(mapVoicePhrase("现在学得怎么样，还有多久").kind ==
        CommandKind::QueryRemainingTime);
  CHECK(mapVoicePhrase("这周的进度怎么样").kind == CommandKind::QueryTodayProgress);
  return true;
}

static bool run_case_stt_unrecognized_and_empty() {
  const auto a = mapVoicePhrase("随便聊聊");
  CHECK(!a.recognized);
  CHECK(a.kind == CommandKind::Unknown);
  const auto b = mapVoicePhrase("");
  CHECK(!b.recognized);
  const auto c = mapVoicePhrase("   ");
  CHECK(!c.recognized);
  return true;
}

static bool run_case_all() {
  CASE(touch_start_maps_intent_and_id);
  CASE(touch_pause_resume_skip_mappings);
  CASE(touch_complete_emits_directly);
  CASE(voice_complete_requires_confirmation_no_emit);
  CASE(mcp_complete_requires_confirmation_no_emit);
  CASE(repeated_mcp_complete_does_not_duplicate);
  CASE(confirm_without_pending_rejected);
  CASE(cancel_clears_pending);
  CASE(query_kinds_never_mutate);
  CASE(unknown_kind_rejected);
  CASE(touch_complete_consumes_pending_ai_request);
  CASE(stt_exact_phrases);
  CASE(stt_complete_not_hijacked_by_query_keyword);
  CASE(stt_keyword_fallback);
  CASE(stt_unrecognized_and_empty);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-LEARNING-V4 P14 interaction dispatcher tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
