// claw4/firmware/main/interaction/interaction_arbiter.cpp
#include "interaction/interaction_arbiter.h"

namespace claw4 {
namespace interaction {

void InteractionArbiter::beginSession(const int64_t generation) {
  generation_ = generation;
  // A closed page/session must not leave a local ring holding the speaker.
  playing_ = false;
  critical_preempted_normal_ = false;
  deferred_.reset();
  deferred_deadline_ms_ = 0;
}

ArbiterVerdict InteractionArbiter::requestPlayback(
    const InteractionRequest& request, const int64_t now_ms) {
  // "关闭会话/切页后的迟到播报/STT被拒": a message prepared against a closed
  // session generation must not act on the new one.
  if (!isLive(request)) return ArbiterVerdict::Denied;

  // System critical always wins. If it preempts a normal source we remember
  // that so the shell can recover that source afterwards.
  if (request.source == AudioSource::SystemCritical) {
    if (playing_ && current_ != AudioSource::SystemCritical) {
      critical_preempted_normal_ = true;
    }
    playing_ = true;
    current_ = AudioSource::SystemCritical;
    return ArbiterVerdict::Granted;
  }

  // Idle speaker: first come, first served (still one source only).
  if (!playing_) {
    playing_ = true;
    current_ = request.source;
    return ArbiterVerdict::Granted;
  }

  // A due local reminder must not be dropped while the network speaks; it gets
  // the 3 s budget and preempts if the budget expires. Critical output is never
  // preempted by a reminder.
  if (request.source == AudioSource::LocalReminder &&
      current_ != AudioSource::SystemCritical) {
    if (!deferred_) {
      deferred_ = request;
      deferred_deadline_ms_ = now_ms + kLocalReminderBudgetMs;
    }
    return ArbiterVerdict::Deferred;
  }

  // Cloud TTS and proactive AI never preempt and never double-play.
  return ArbiterVerdict::Deferred;
}

bool InteractionArbiter::playbackFinished(const int64_t now_ms) {
  playing_ = false;
  critical_preempted_normal_ = false;

  // A deferred local reminder that still has budget starts immediately.
  if (deferred_ && now_ms <= deferred_deadline_ms_) {
    playing_ = true;
    current_ = AudioSource::LocalReminder;
    deferred_.reset();
    deferred_deadline_ms_ = 0;
    return true;
  }
  return false;
}

bool InteractionArbiter::expireDeferredReminder(const int64_t now_ms) {
  if (!deferred_) return false;
  if (now_ms <= deferred_deadline_ms_) return false;

  // Budget exhausted: the LOCAL ring takes the speaker away from the network
  // playback. A reminder does not wait for the network.
  deferred_.reset();
  deferred_deadline_ms_ = 0;
  critical_preempted_normal_ = false;
  playing_ = true;
  current_ = AudioSource::LocalReminder;
  return true;
}

ArbiterVerdict InteractionArbiter::authorizeReminderAction(
    const InteractionRequest& request, const ReminderScope& scope,
    const ReminderAction action) const {
  (void)action;  // Snooze / Acknowledge / Dismiss share one authorization gate
  if (!isLive(request)) return ArbiterVerdict::Denied;
  // A proactive AI utterance can never act on the child's behalf.
  if (request.source == AudioSource::ProactiveAi) return ArbiterVerdict::Denied;
  // Only an explicit user command may Snooze / ACK / Dismiss.
  if (!request.explicit_user_intent) return ArbiterVerdict::Denied;
  // The command must name the exact reminder instance it acts on.
  if (!scope.matches(request)) return ArbiterVerdict::Denied;
  return ArbiterVerdict::Granted;
}

ArbiterVerdict InteractionArbiter::authorizeTaskCompletion(
    const InteractionRequest& request, const ReminderScope& scope,
    const bool physical_confirmation) const {
  if (!isLive(request)) return ArbiterVerdict::Denied;
  if (request.source == AudioSource::ProactiveAi) return ArbiterVerdict::Denied;
  if (!request.explicit_user_intent) return ArbiterVerdict::Denied;
  if (!scope.matches(request)) return ArbiterVerdict::Denied;
  // ACK is NOT Complete: completing a task additionally needs a physical
  // confirmation bound to the current task.
  if (!physical_confirmation) return ArbiterVerdict::Denied;
  return ArbiterVerdict::Granted;
}

}  // namespace interaction
}  // namespace claw4
