// CODEX-APP-FIRST-001 AF2 — bridge BackendClient into AppCoordinator.
#pragma once

#include "application/coordinator.h"
#include "sync/backend_client.h"

namespace claw4 {
namespace sync {

// Thin production boundary: coordinator owns retry/ACK policy; BackendClient
// owns endpoint JSON and this adapter only forwards the request.
class BackendSyncTransport final : public application::SyncTransport {
 public:
  explicit BackendSyncTransport(BackendClient& client) : client_(client) {}

  SyncClient::Response send(const SyncClient::Request& request) override {
    return client_.syncBatch(request);
  }

 private:
  BackendClient& client_;
};

}  // namespace sync
}  // namespace claw4
