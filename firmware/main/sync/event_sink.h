// claw4/firmware/main/sync/event_sink.h
// EventSink: the boundary through which domain events enter the sync layer.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §3.4 / §7.3.
#pragma once

#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"

namespace claw4 {
namespace sync {

// Persists a domain event into the local event queue together with the domain
// snapshot in one atomic sequence (outbox, ARCHITECTURE.md §7.3.1). The sync
// layer MUST NOT implement network or persistence here — this checkpoint only
// fixes the interface.
struct PendingTransition {
  claw4::domain::DomainState next_state;
  std::vector<claw4::domain::DeviceEvent> events;
};

class EventSink {
 public:
  virtual ~EventSink() = default;

  // Atomically persists the complete next domain snapshot and all events
  // produced by the transition. Returns false without committing either side
  // when persistence fails. UI publication happens only after true (I8).
  virtual bool persistTransition(const PendingTransition& transition) = 0;
};

}  // namespace sync
}  // namespace claw4
