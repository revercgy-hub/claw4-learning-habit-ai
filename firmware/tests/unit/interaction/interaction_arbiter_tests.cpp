// claw4/firmware/tests/unit/interaction/interaction_arbiter_tests.cpp
// WB-V53-NEXT-001 CP3 (B02) — host tests for the audio/reminder arbiter.
// Deterministic: no threads, no sleeps, no real audio, no AudioService.

#include <cstdint>
#include <cstdio>
#include <string>

#include "interaction/interaction_arbiter.h"

using namespace claw4::domain;
using namespace claw4::interaction;

static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                             \
  do {                                         \
    ++g_cases;                                 \
    if (!run_case_##name()) {                  \
      std::printf("FAIL: %s\n", #name);        \
      ++g_fail;                                \
    }                                          \
  } while (0)

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      std::printf("  assert fail: %s (line %d)\n", #cond, __LINE__);  \
      return false;                                                   \
    }                                                                 \
  } while (0)

// ---------------------------------------------------------------------------
static ReminderScope scopeFor(int64_t generation,
                             const std::string& instance = "rem-1") {
  ReminderScope s;
  s.child_id = ChildId{"child-1"};
  s.task_id = TaskId{"task-1"};
  s.reminder_instance_id = instance;
  s.generation = generation;
  return s;
}

static InteractionRequest voiceFromUser(AudioSource source, int64_t generation,
                                        const std::string& instance = "rem-1") {
  InteractionRequest r;
  r.source = source;
  r.child_id = ChildId{"child-1"};
  r.task_id = TaskId{"task-1"};
  r.reminder_instance_id = instance;
  r.generation = generation;
  r.explicit_user_intent = true;  // the child's own command
  return r;
}

static InteractionRequest proactiveAi(int64_t generation) {
  InteractionRequest r;
  r.source = AudioSource::ProactiveAi;
  r.child_id = ChildId{"child-1"};
  r.task_id = TaskId{"task-1"};
  r.reminder_instance_id = "rem-1";
  r.generation = generation;
  r.explicit_user_intent = false;  // assistant-initiated: no user intent
  return r;
}

// ---------------------------------------------------------------------------
static bool run_case_critical_preempts_and_is_recoverable() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.playing());
  CHECK(arbiter.currentSource() == AudioSource::CloudTts);

  // System critical preempts immediately, single slot preserved.
  InteractionRequest critical;
  critical.source = AudioSource::SystemCritical;
  critical.generation = 1;
  CHECK(arbiter.requestPlayback(critical, 10) == ArbiterVerdict::Granted);
  CHECK(arbiter.playing());
  CHECK(arbiter.currentSource() == AudioSource::SystemCritical);
  CHECK(arbiter.normalSourcePreempted());  // something to recover

  // Critical finishes; the shell can re-issue the normal source.
  CHECK(!arbiter.playbackFinished(20));
  CHECK(!arbiter.playing());
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 21) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.currentSource() == AudioSource::CloudTts);
  return true;
}

static bool run_case_local_reminder_budget_then_preempts() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);

  // A due local reminder must not be dropped while the network speaks.
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 100) ==
        ArbiterVerdict::Deferred);
  CHECK(arbiter.reminderDeferred());
  CHECK(arbiter.deferredDeadlineMs() ==
        100 + InteractionArbiter::kLocalReminderBudgetMs);
  CHECK(arbiter.currentSource() == AudioSource::CloudTts);  // still TTS

  // Inside the budget nothing is preempted.
  CHECK(!arbiter.expireDeferredReminder(100 + InteractionArbiter::kLocalReminderBudgetMs));
  CHECK(arbiter.currentSource() == AudioSource::CloudTts);

  // Budget exhausted: the LOCAL ring takes the speaker from the network.
  CHECK(arbiter.expireDeferredReminder(100 +
                                       InteractionArbiter::kLocalReminderBudgetMs + 1));
  CHECK(arbiter.playing());
  CHECK(arbiter.currentSource() == AudioSource::LocalReminder);
  CHECK(!arbiter.reminderDeferred());
  return true;
}

static bool run_case_deferred_reminder_starts_when_tts_finishes_in_budget() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 500) ==
        ArbiterVerdict::Deferred);
  // TTS finishes with 1 ms of budget to spare: the reminder starts at once.
  CHECK(arbiter.playbackFinished(500 + InteractionArbiter::kLocalReminderBudgetMs));
  CHECK(arbiter.playing());
  CHECK(arbiter.currentSource() == AudioSource::LocalReminder);
  CHECK(!arbiter.reminderDeferred());
  return true;
}

static bool run_case_deferred_reminder_dropped_when_tts_finishes_late() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 500) ==
        ArbiterVerdict::Deferred);
  // TTS overran the budget: it does not get to hand the speaker to the
  // reminder silently; the shell must use expireDeferredReminder() explicitly.
  CHECK(!arbiter.playbackFinished(500 + InteractionArbiter::kLocalReminderBudgetMs + 1));
  CHECK(!arbiter.playing());
  return true;
}

static bool run_case_never_two_sources_at_once() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  // A second cloud TTS may not start while the speaker is busy.
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 1) ==
        ArbiterVerdict::Deferred);
  CHECK(arbiter.playing());
  CHECK(arbiter.currentSource() == AudioSource::CloudTts);
  // Proactive AI never preempts either.
  CHECK(arbiter.requestPlayback(proactiveAi(1), 2) == ArbiterVerdict::Deferred);
  CHECK(arbiter.currentSource() == AudioSource::CloudTts);
  return true;
}

static bool run_case_late_message_from_closed_session_is_denied() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  // The learning page is closed / the session is rebuilt.
  arbiter.beginSession(2);
  CHECK(!arbiter.playing());
  // A late TTS prepared against generation 1 must be refused, not played.
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 10) ==
        ArbiterVerdict::Denied);
  CHECK(!arbiter.playing());
  // A message for the NEW session is accepted.
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 2), 11) ==
        ArbiterVerdict::Granted);
  return true;
}

static bool run_case_session_reset_clears_deferred_reminder() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 10) ==
        ArbiterVerdict::Deferred);
  arbiter.beginSession(2);
  CHECK(!arbiter.reminderDeferred());
  CHECK(!arbiter.playing());
  CHECK(arbiter.deferredDeadlineMs() == 0);
  return true;
}

static bool run_case_reminder_action_requires_intent_and_scope() {
  InteractionArbiter arbiter(1);
  const ReminderScope scope = scopeFor(1);
  const InteractionRequest user_cmd = voiceFromUser(AudioSource::LocalReminder, 1);

  // Explicit user command + matching scope + live generation -> granted.
  CHECK(arbiter.authorizeReminderAction(user_cmd, scope, ReminderAction::Snooze) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.authorizeReminderAction(user_cmd, scope, ReminderAction::Acknowledge) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.authorizeReminderAction(user_cmd, scope, ReminderAction::Dismiss) ==
        ArbiterVerdict::Granted);

  // No explicit user intent -> denied.
  InteractionRequest no_intent = user_cmd;
  no_intent.explicit_user_intent = false;
  CHECK(arbiter.authorizeReminderAction(no_intent, scope, ReminderAction::Snooze) ==
        ArbiterVerdict::Denied);

  // Scope mismatch (wrong reminder instance) -> denied.
  CHECK(arbiter.authorizeReminderAction(user_cmd, scopeFor(1, "rem-2"),
                                        ReminderAction::Snooze) ==
        ArbiterVerdict::Denied);
  // Scope mismatch (wrong generation) -> denied.
  CHECK(arbiter.authorizeReminderAction(user_cmd, scopeFor(2),
                                        ReminderAction::Snooze) ==
        ArbiterVerdict::Denied);
  // Wrong child / task -> denied.
  ReminderScope other_child = scope;
  other_child.child_id = ChildId{"child-2"};
  CHECK(arbiter.authorizeReminderAction(user_cmd, other_child,
                                        ReminderAction::Acknowledge) ==
        ArbiterVerdict::Denied);
  ReminderScope other_task = scope;
  other_task.task_id = TaskId{"task-9"};
  CHECK(arbiter.authorizeReminderAction(user_cmd, other_task,
                                        ReminderAction::Dismiss) ==
        ArbiterVerdict::Denied);

  // Late command after the session moved on -> denied.
  InteractionRequest stale = user_cmd;
  stale.generation = 0;
  CHECK(arbiter.authorizeReminderAction(stale, scope, ReminderAction::Snooze) ==
        ArbiterVerdict::Denied);
  return true;
}

static bool run_case_proactive_ai_cannot_authorize_anything() {
  InteractionArbiter arbiter(1);
  const ReminderScope scope = scopeFor(1);
  // Even with a forged explicit_user_intent flag, a proactive source is denied.
  InteractionRequest forged = proactiveAi(1);
  forged.explicit_user_intent = true;
  CHECK(arbiter.authorizeReminderAction(forged, scope, ReminderAction::Snooze) ==
        ArbiterVerdict::Denied);
  CHECK(arbiter.authorizeReminderAction(forged, scope, ReminderAction::Acknowledge) ==
        ArbiterVerdict::Denied);
  CHECK(arbiter.authorizeTaskCompletion(forged, scope, /*physical_confirmation=*/true) ==
        ArbiterVerdict::Denied);
  return true;
}

static bool run_case_ack_is_not_completion() {
  InteractionArbiter arbiter(1);
  const ReminderScope scope = scopeFor(1);
  const InteractionRequest user_cmd = voiceFromUser(AudioSource::LocalReminder, 1);

  // The child acknowledges the reminder: allowed, but it must NOT complete.
  CHECK(arbiter.authorizeReminderAction(user_cmd, scope, ReminderAction::Acknowledge) ==
        ArbiterVerdict::Granted);
  // Completion still needs the physical confirmation bound to the task.
  CHECK(arbiter.authorizeTaskCompletion(user_cmd, scope,
                                        /*physical_confirmation=*/false) ==
        ArbiterVerdict::Denied);
  CHECK(arbiter.authorizeTaskCompletion(user_cmd, scope,
                                        /*physical_confirmation=*/true) ==
        ArbiterVerdict::Granted);
  // Missing explicit intent denies completion even with a physical flag.
  InteractionRequest no_intent = user_cmd;
  no_intent.explicit_user_intent = false;
  CHECK(arbiter.authorizeTaskCompletion(no_intent, scope, true) ==
        ArbiterVerdict::Denied);
  return true;
}

// ---------------------------------------------------------------------------
// REVIEW-FIX-001 RF4: SystemCritical is above a deferred reminder in EVERY
// combination — expiry must never demote critical.
// ---------------------------------------------------------------------------

static bool run_case_critical_blocks_deferred_reminder_expiry() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  // A local reminder is deferred with its 3 s budget.
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 100) ==
        ArbiterVerdict::Deferred);
  const int64_t deadline = arbiter.deferredDeadlineMs();
  CHECK(deadline == 100 + InteractionArbiter::kLocalReminderBudgetMs);

  // Critical preempts the TTS; the reminder is now parked behind critical.
  InteractionRequest critical;
  critical.source = AudioSource::SystemCritical;
  critical.generation = 1;
  CHECK(arbiter.requestPlayback(critical, 200) == ArbiterVerdict::Granted);
  CHECK(arbiter.currentSource() == AudioSource::SystemCritical);
  CHECK(arbiter.deferredBlockedByCritical());
  CHECK(arbiter.reminderDeferred());          // still pending, nothing lost
  CHECK(arbiter.deferredDeadlineMs() == 0);   // no budget against critical

  // Even far past the old budget, expiry must NOT preempt critical.
  CHECK(!arbiter.expireDeferredReminder(deadline + 100000));
  CHECK(arbiter.currentSource() == AudioSource::SystemCritical);
  CHECK(arbiter.playing());
  CHECK(arbiter.reminderDeferred());
  return true;
}

static bool run_case_deferred_reminder_runs_after_critical_finishes() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 100) ==
        ArbiterVerdict::Deferred);
  InteractionRequest critical;
  critical.source = AudioSource::SystemCritical;
  critical.generation = 1;
  CHECK(arbiter.requestPlayback(critical, 200) == ArbiterVerdict::Granted);
  CHECK(arbiter.deferredBlockedByCritical());

  // Critical finishes long after the original budget: the reminder is restored.
  CHECK(arbiter.playbackFinished(100 + InteractionArbiter::kLocalReminderBudgetMs + 5000));
  CHECK(arbiter.playing());
  CHECK(arbiter.currentSource() == AudioSource::LocalReminder);
  CHECK(!arbiter.reminderDeferred());
  CHECK(!arbiter.deferredBlockedByCritical());
  CHECK(!arbiter.normalSourcePreempted());
  return true;
}

static bool run_case_session_reset_during_critical_clears_stale_deferred() {
  InteractionArbiter arbiter(1);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::CloudTts, 1), 0) ==
        ArbiterVerdict::Granted);
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 10) ==
        ArbiterVerdict::Deferred);
  InteractionRequest critical;
  critical.source = AudioSource::SystemCritical;
  critical.generation = 1;
  CHECK(arbiter.requestPlayback(critical, 20) == ArbiterVerdict::Granted);
  CHECK(arbiter.deferredBlockedByCritical());

  // The page is closed while critical is still playing.
  arbiter.beginSession(2);
  CHECK(!arbiter.reminderDeferred());
  CHECK(!arbiter.deferredBlockedByCritical());
  CHECK(!arbiter.playing());
  CHECK(arbiter.deferredDeadlineMs() == 0);
  // Nothing from the old generation can revive it.
  CHECK(arbiter.requestPlayback(voiceFromUser(AudioSource::LocalReminder, 1), 30) ==
        ArbiterVerdict::Denied);
  CHECK(!arbiter.reminderDeferred());
  return true;
}

static bool run_case_all() {
  CASE(critical_preempts_and_is_recoverable);
  CASE(local_reminder_budget_then_preempts);
  CASE(deferred_reminder_starts_when_tts_finishes_in_budget);
  CASE(deferred_reminder_dropped_when_tts_finishes_late);
  CASE(never_two_sources_at_once);
  CASE(late_message_from_closed_session_is_denied);
  CASE(session_reset_clears_deferred_reminder);
  CASE(reminder_action_requires_intent_and_scope);
  CASE(proactive_ai_cannot_authorize_anything);
  CASE(ack_is_not_completion);
  // REVIEW-FIX-001 RF4
  CASE(critical_blocks_deferred_reminder_expiry);
  CASE(deferred_reminder_runs_after_critical_finishes);
  CASE(session_reset_during_critical_clears_stale_deferred);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 CP3 interaction arbiter (B02) tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
