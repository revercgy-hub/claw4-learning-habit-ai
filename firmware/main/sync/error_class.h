// claw4/firmware/main/sync/error_class.h
// Classifies sync failures so the retry/dead-letter policy can be applied.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §6.7 / §7.3.3.
#pragma once

#include <cstdint>

namespace claw4 {
namespace sync {

// Error classification used by the retry policy (ARCHITECTURE.md §6.7).
enum class SyncErrorClass : uint8_t {
  None = 0,
  Auth,          // 401/403: NOT a per-event dead-letter condition; pause sync, events stay pending
  Network,       // 5xx / DNS / timeout: retry with exponential backoff
  Business,      // business 4xx: critical events kept verbatim, replayable with same event_id
  Conflict,      // same event_id with different payload: reject + alert, kept pending
  Server,        // 5xx server: retry with backoff
  Unknown,
};

}  // namespace sync
}  // namespace claw4
