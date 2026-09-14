// claw4/firmware/main/interaction/interaction_arbiter.h
// WB-V53-NEXT-001 CP3 (B02): audio + reminder arbitration, pure Host logic.
//
// This is a deterministic state machine. It never touches the official
// AudioService, never plays real audio and never performs I/O: the device shell
// (later, C03) asks it for a verdict and then drives the platform player.
//
// Contracts enforced here (work package B02):
//   * SystemCritical always wins and can be recovered from;
//   * a normal cloud TTS may hold the speaker for at most the 3 s local
//     reminder budget — after that a due local reminder PREEMPTS it, because a
//     local ring must never wait on the network;
//   * two sources never play at the same time (single `playing_` slot);
//   * messages from a superseded session generation (page closed / session
//     rebuilt) are refused, so a late TTS/STT result cannot act on the new UI;
//   * Snooze / ACK / Dismiss need an explicit user intent AND a matching
//     child/task/reminder-instance/generation scope;
//   * an ACK is never a task completion: completion additionally requires a
//     physical confirmation;
//   * a proactive AI utterance can never authorize a reminder action or a
//     completion on the child's behalf.
//
// Portable C++17. No LVGL / ESP-IDF / FreeRTOS / BSP dependencies.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "learning_domain/ids.h"

namespace claw4 {
namespace interaction {

// Where a playback request comes from. Ordered by preemption power.
enum class AudioSource : uint8_t {
  SystemCritical = 0,  // watchdog / power / safety prompt
  LocalReminder,       // local ring for a due reminder (must not wait on net)
  CloudTts,            // normal assistant speech
  ProactiveAi,         // assistant-initiated speech (lowest, cannot authorize)
};

enum class ArbiterVerdict : uint8_t {
  Granted = 0,  // may start now / action authorized
  Deferred,     // not now; caller must not start it
  Denied,       // stale session, missing intent, or scope mismatch
};

enum class ReminderAction : uint8_t {
  Snooze = 0,
  Acknowledge,  // confirms the reminder was seen; NOT a task completion
  Dismiss,
};

struct InteractionRequest {
  AudioSource source = AudioSource::CloudTts;
  claw4::domain::ChildId child_id;
  claw4::domain::TaskId task_id;
  std::string reminder_instance_id;  // stable per-reminder instance id
  int64_t generation = 0;            // session generation this message belongs to
  // True ONLY when the request originates from the child's own explicit
  // command (touch/voice). Assistant-initiated speech must leave it false.
  bool explicit_user_intent = false;
};

// The reminder a user command is allowed to act on.
struct ReminderScope {
  claw4::domain::ChildId child_id;
  claw4::domain::TaskId task_id;
  std::string reminder_instance_id;
  int64_t generation = 0;

  bool matches(const InteractionRequest& request) const {
    return request.generation == generation && request.child_id == child_id &&
           request.task_id == task_id &&
           request.reminder_instance_id == reminder_instance_id;
  }
};

class InteractionArbiter {
 public:
  // "普通 TTS 等待本地提醒最多 3 秒预算" (work package B02 acceptance).
  static constexpr int64_t kLocalReminderBudgetMs = 3000;

  explicit InteractionArbiter(int64_t generation = 0) : generation_(generation) {}

  int64_t generation() const { return generation_; }

  // Session/page lifecycle. Bumping the generation invalidates every message
  // prepared against the previous one.
  void beginSession(int64_t generation);

  // --- audio arbitration -------------------------------------------------
  // Never grants two sources at once. A local reminder during a normal TTS is
  // DEFERRED with a 3 s budget instead of being dropped.
  ArbiterVerdict requestPlayback(const InteractionRequest& request, int64_t now_ms);

  // The current playback ended at `now_ms`. Returns true when a deferred local
  // reminder may start IMMEDIATELY (it then owns the single playing slot).
  bool playbackFinished(int64_t now_ms);

  // The 3 s budget of a deferred local reminder expired at `now_ms`: the local
  // ring preempts the network playback. Returns true when preemption happened.
  bool expireDeferredReminder(int64_t now_ms);

  bool playing() const { return playing_; }
  AudioSource currentSource() const { return current_; }
  bool reminderDeferred() const { return deferred_.has_value(); }
  int64_t deferredDeadlineMs() const { return deferred_deadline_ms_; }
  // Set while a critical prompt is holding the speaker after preempting a
  // normal source, so the shell knows there is something to recover.
  bool normalSourcePreempted() const { return critical_preempted_normal_; }

  // --- authorization -----------------------------------------------------
  // Snooze / ACK / Dismiss: requires the child's own explicit intent, a
  // matching scope and a live generation. A proactive AI request is always
  // denied — it can never authorize on the child's behalf.
  ArbiterVerdict authorizeReminderAction(const InteractionRequest& request,
                                         const ReminderScope& scope,
                                         ReminderAction action) const;

  // Task completion: same gates PLUS an explicit physical confirmation. An ACK
  // alone never completes a task.
  ArbiterVerdict authorizeTaskCompletion(const InteractionRequest& request,
                                        const ReminderScope& scope,
                                        bool physical_confirmation) const;

 private:
  bool isLive(const InteractionRequest& request) const {
    return request.generation == generation_;
  }

  int64_t generation_ = 0;
  bool playing_ = false;
  AudioSource current_ = AudioSource::CloudTts;
  bool critical_preempted_normal_ = false;
  std::optional<InteractionRequest> deferred_;
  int64_t deferred_deadline_ms_ = 0;
};

}  // namespace interaction
}  // namespace claw4
