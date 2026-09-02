// claw4/firmware/main/sync/event_sink.h
// EventSink: the boundary through which domain events enter the sync layer.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §3.4 / §7.3.
#pragma once

#include "learning_domain/event.h"

namespace claw4 {
namespace sync {

// Persists a domain event into the local event queue together with the domain
// snapshot in one atomic sequence (outbox, ARCHITECTURE.md §7.3.1). The sync
// layer MUST NOT implement network or persistence here — this checkpoint only
// fixes the interface.
class EventSink {
 public:
  virtual ~EventSink() = default;

  // Returns false if the event could not be persisted. Callers MUST NOT
  // commit the domain state change in that case (I8).
  virtual bool persist(const claw4::domain::DeviceEvent& event) = 0;
};

}  // namespace sync
}  // namespace claw4
