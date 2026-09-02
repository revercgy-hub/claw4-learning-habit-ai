// claw4/firmware/main/sync/sync_client.h
// SyncClient: boundary for talking to the home backend /events/batch.
// Portable C++17. No hardware dependencies.
// Contract source: ARCHITECTURE.md §6.2/§6.3/§7.4.
#pragma once

#include <cstdint>
#include <vector>

#include "learning_domain/event.h"
#include "learning_domain/ids.h"
#include "sync/batch_result.h"
#include "sync/error_class.h"

namespace claw4 {
namespace sync {

// Sends a batch of events to the backend. Implementations are written in a
// later checkpoint (T6 event chain). This checkpoint only fixes the contract:
// a per-event result list and the consecutive ACK must be returned so the
// client never deletes pending events outside the acknowledged prefix.
class SyncClient {
 public:
  virtual ~SyncClient() = default;

  struct Request {
    claw4::domain::DeviceId device_id;
    int64_t last_acked_sequence = 0;  // consecutive prefix known to the client
    std::vector<claw4::domain::DeviceEvent> events;
  };

  struct Response {
    SyncErrorClass error_class = SyncErrorClass::None;
    int http_status = 0;
    BatchSyncResult batch;
  };

  virtual Response syncBatch(const Request& request) = 0;
};

}  // namespace sync
}  // namespace claw4
