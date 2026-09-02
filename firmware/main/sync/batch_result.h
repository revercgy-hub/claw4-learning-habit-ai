// claw4/firmware/main/sync/batch_result.h
// Per-event sync results and the consecutive ACK contract.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §5.4 / §6.3 / CR-WB002-02/06.
#pragma once

#include <cstdint>
#include <vector>

#include "learning_domain/event.h"
#include "learning_domain/ids.h"

namespace claw4 {
namespace sync {

// Per-event outcome returned by the backend (ARCHITECTURE.md §6.3 results[]).
enum class EventOutcome : uint8_t {
  Accepted = 0,   // persisted and business-processed; part of the consecutive prefix
  Duplicate,      // (device_id, event_id) already exists with identical digest; success semantics
  Conflict,       // same event_id but different sequence/type/payload digest; rejected + alert
  Rejected,       // business 4xx or sequence regression; kept pending
  Gap,            // new event with sequence > last_acked+1; kept pending, ACK not advanced
};

struct PerEventResult {
  claw4::domain::EventId event_id;  // fully qualified: EventId lives in claw4::domain
  int64_t sequence = 0;
  EventOutcome outcome = EventOutcome::Rejected;
  int http_status = 0;
};

// Batch sync response (ARCHITECTURE.md §6.3).
struct BatchSyncResult {
  // Highest consecutive, persisted and business-processed sequence. A client
  // may only drop pending events that are (a) accepted/duplicate AND
  // (b) inside this consecutive prefix.
  int64_t last_acked_sequence = 0;
  int64_t server_time = 0;
  std::vector<PerEventResult> results;
};

}  // namespace sync
}  // namespace claw4
