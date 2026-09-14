// claw4/firmware/main/sync/single_flight_http_transport.cpp
#include "sync/single_flight_http_transport.h"

#include <utility>

namespace claw4 {
namespace sync {

SingleFlightHttpTransport::SingleFlightHttpTransport(BlockingRequestFn request)
    : request_(std::move(request)) {}

HttpResponse SingleFlightHttpTransport::request(
    const std::string& method, const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body, const int64_t timeout_ms) {
  if (!request_ || timeout_ms < 0) return {};

  {
    // Single-flight gate. The lock is released BEFORE the blocking call so a
    // concurrent UI/local-command path is never serialized behind the network.
    std::lock_guard<std::mutex> lock(mutex_);
    if (in_flight_) {
      ++rejected_busy_;
      return {};  // -> SyncErrorClass::Network -> bounded Backoff upstream
    }
    in_flight_ = true;
  }

  HttpResponse response = request_(method, url, headers, body, timeout_ms);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    in_flight_ = false;
  }
  return response;
}

int SingleFlightHttpTransport::rejectedBusy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return rejected_busy_;
}

bool SingleFlightHttpTransport::inFlight() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return in_flight_;
}

}  // namespace sync
}  // namespace claw4
