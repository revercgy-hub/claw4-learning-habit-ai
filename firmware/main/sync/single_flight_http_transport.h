// claw4/firmware/main/sync/single_flight_http_transport.h
// WB-V53-NEXT-001 CP1 (A03): blocking HTTP boundary with an exclusive client.
//
// Why this exists (ADR V53_A03_RUNTIME_CONCURRENCY_ADR §1/§2):
//   * the previous device boundary (ScheduledHttpTransport + Application::
//     Schedule) deferred the REAL DNS/connect/read/close work onto the LVGL
//     main loop and then blocked the caller waiting for that callback, so the
//     main loop itself executed the blocking I/O and the UI froze;
//   * EspNetwork::CreateHttp() hands out an independent HttpClient, but one
//     HttpClient must never be shared or concurrently closed across tasks.
//
// This transport therefore runs the injected RequestFn on the CALLER's task
// (the device shell only calls it from the backend worker) and enforces
// single-flight: while one request is in flight any further call is rejected
// with an empty response instead of racing the shared client. An empty
// response maps to SyncErrorClass::Network upstream, i.e. a bounded Backoff —
// never an unbounded queue and never a second concurrent client.
//
// Portable C++17. No LVGL / ESP-IDF / FreeRTOS / BSP dependencies.
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "sync/http_transport.h"

namespace claw4 {
namespace sync {

// Blocking, platform-neutral request implementation (device adapter owns
// sockets/TLS; the host test injects a fake). Must not be called from a UI or
// main-loop context.
using BlockingRequestFn =
    std::function<HttpResponse(const std::string& method, const std::string& url,
                               const std::vector<std::pair<std::string, std::string>>& headers,
                               const std::string& body, int64_t timeout_ms)>;

class SingleFlightHttpTransport final : public HttpTransport {
 public:
  explicit SingleFlightHttpTransport(BlockingRequestFn request);

  // Runs `request_` inline on the calling task with no internal queue. A second
  // concurrent (or re-entrant) call is rejected immediately with a default
  // HttpResponse (transport_ok == false) and counted in rejectedBusy().
  HttpResponse request(const std::string& method, const std::string& url,
                       const std::vector<std::pair<std::string, std::string>>& headers,
                       const std::string& body, int64_t timeout_ms) override;

  // Number of calls rejected because a request was already in flight.
  int rejectedBusy() const;
  // Whether a request is in flight right now (diagnostics only).
  bool inFlight() const;

 private:
  BlockingRequestFn request_;
  mutable std::mutex mutex_;
  bool in_flight_ = false;
  int rejected_busy_ = 0;
};

}  // namespace sync
}  // namespace claw4
