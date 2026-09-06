// CODEX-APP-FIRST-001 AF3 — platform-neutral main-loop HTTP boundary.
// The socket implementation is injected; the scheduler is the only path
// that may touch a platform network object. This keeps BackendClient unaware
// of LVGL, FreeRTOS and the vendor board API while still proving dispatch.
#pragma once

#include <functional>
#include <memory>

#include "sync/http_transport.h"

namespace claw4::sync {

class ScheduledHttpTransport final : public HttpTransport {
 public:
  using ScheduleFn = std::function<void(std::function<void()>)>;
  using RequestFn = std::function<HttpResponse(
      const std::string&, const std::string&,
      const std::vector<std::pair<std::string, std::string>>&, const std::string&, int64_t)>;

  ScheduledHttpTransport(ScheduleFn schedule, RequestFn request)
      : schedule_(std::move(schedule)), request_(std::move(request)) {}

  HttpResponse request(const std::string& method, const std::string& url,
                       const std::vector<std::pair<std::string, std::string>>& headers,
                       const std::string& body, int64_t timeout_ms) override;

 private:
  ScheduleFn schedule_;
  RequestFn request_;
};

}  // namespace claw4::sync
