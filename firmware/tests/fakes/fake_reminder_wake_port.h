// claw4/firmware/tests/fakes/fake_reminder_wake_port.h
// REVIEW-FIX-001 RF5.6 — deterministic fake for the platform wake port.
// Records the requested deadline so a Host test can assert the reminder core
// schedules/updates/cancels the platform wake. No ESP-IDF, no esp_sleep.
#pragma once

#include <cstdint>

#include "ports/reminder_wake_port.h"

namespace claw4 {
namespace fakes {

class FakeReminderWakePort final : public ports::ReminderWakePort {
 public:
  int64_t mono_ms = 0;
  int64_t scheduled_deadline_ms = 0;
  bool wake_armed = false;
  int schedule_calls = 0;
  int cancel_calls = 0;

  int64_t monotonicMs() override { return mono_ms; }

  bool scheduleWakeAt(int64_t monotonic_deadline_ms) override {
    ++schedule_calls;
    scheduled_deadline_ms = monotonic_deadline_ms;
    wake_armed = true;
    return true;
  }

  void cancelWake() override {
    ++cancel_calls;
    wake_armed = false;
    scheduled_deadline_ms = 0;
  }
};

}  // namespace fakes
}  // namespace claw4
