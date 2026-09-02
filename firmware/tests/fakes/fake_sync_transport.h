// claw4/firmware/tests/fakes/fake_sync_transport.h
// Programmable sync transport fake for host coordinator tests.
// Never performs real network I/O.
#pragma once

#include <functional>
#include <vector>

#include "application/coordinator.h"
#include "sync/sync_client.h"

namespace claw4 {
namespace fakes {

// A transport whose response is programmable per test. Records every request
// so tests can assert what the coordinator actually sent.
class FakeSyncTransport final : public application::SyncTransport {
 public:
  // Default: everything accepted, ACK advanced to the last sent sequence.
  std::function<sync::SyncClient::Response()> on_send;

  FakeSyncTransport() {
    on_send = []() -> sync::SyncClient::Response {
      sync::SyncClient::Response r;
      r.error_class = sync::SyncErrorClass::None;
      r.http_status = 200;
      return r;
    };
  }

  sync::SyncClient::Response send(const sync::SyncClient::Request& request) override {
    requests.push_back(request);
    sync::SyncClient::Response r = on_send();
    // If the test did not customize results, accept everything sent and ACK
    // the consecutive prefix.
    if (r.batch.results.empty() && r.error_class == sync::SyncErrorClass::None) {
      int64_t ack = request.last_acked_sequence;
      for (const auto& e : request.events) {
        sync::PerEventResult per;
        per.event_id = e.event_id;
        per.sequence = e.sequence;
        per.outcome = sync::EventOutcome::Accepted;
        per.http_status = 200;
        r.batch.results.push_back(per);
        ack = e.sequence;
      }
      r.batch.last_acked_sequence = ack;
    }
    return r;
  }

  std::vector<sync::SyncClient::Request> requests;
};

}  // namespace fakes
}  // namespace claw4
