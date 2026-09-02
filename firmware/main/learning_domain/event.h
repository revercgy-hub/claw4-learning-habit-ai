// claw4/firmware/main/learning_domain/event.h
// DeviceEvent value type (unified event envelope).
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §5.3.
#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "learning_domain/ids.h"

namespace claw4 {
namespace domain {

// MVP event type subset (ARCHITECTURE.md §5.3).
enum class EventType : uint8_t {
  DeviceBooted = 0,
  DeviceOnline,
  DeviceOffline,
  TaskStarted,
  TaskPaused,
  TaskResumed,
  TaskCompleted,
  TaskSkipped,
  StudySessionStarted,
  StudySessionCompleted,
  SyncFailed,     // local mergeable diagnostic, NOT enqueued into the business queue (7.3.3)
  SyncRecovered,  // local mergeable diagnostic, NOT enqueued into the business queue (7.3.3)
};

// Timestamp source: `rtc` (SNTP synced) vs `local` (unsynced).
enum class TimestampSource : uint8_t {
  Rtc = 0,
  Local,
};

// Unified event envelope. `event_id` is the idempotency key; `sequence` is the
// per-device monotonic counter (persisted, survives reboot).
struct DeviceEvent {
  EventId event_id;
  DeviceId device_id;
  ChildId child_id;
  int64_t sequence = 0;
  int64_t timestamp = 0;          // unix epoch seconds
  TimestampSource timestamp_source = TimestampSource::Local;
  EventType type = EventType::DeviceBooted;
  int version = 1;                // payload schema version
  // Payload kept as an ordered string map for MVP so the envelope stays
  // trivially serializable and diffable for idempotency/conflict checks.
  std::map<std::string, std::string> payload;
};

// Pure-domain output before transactional persistence. The reducer receives
// event_id and time from its caller so retries can reuse the same idempotency
// key, but it never allocates the persisted per-device sequence. The outbox is
// the single sequence owner and materializes DeviceEvent during atomic commit.
struct EventDraft {
  EventId event_id;
  DeviceId device_id;
  ChildId child_id;
  int64_t timestamp = 0;
  TimestampSource timestamp_source = TimestampSource::Local;
  EventType type = EventType::DeviceBooted;
  int version = 1;
  std::map<std::string, std::string> payload;
};

}  // namespace domain
}  // namespace claw4
