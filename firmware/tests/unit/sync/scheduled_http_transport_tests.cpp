#include <cstdio>
#include <string>

#include "sync/scheduled_http_transport.h"

using claw4::sync::HttpResponse;
using claw4::sync::ScheduledHttpTransport;

int main() {
  int scheduled = 0;
  ScheduledHttpTransport transport(
      [&](std::function<void()> callback) {
        ++scheduled;
        callback();
      },
      [](const std::string& method, const std::string& url,
         const std::vector<std::pair<std::string, std::string>>& headers,
         const std::string& body, int64_t timeout) {
        if (method != "POST" || url != "http://loopback" || headers.size() != 1 ||
            body != "{}" || timeout != 100) return HttpResponse{};
        return HttpResponse{true, 201, "{ok}"};
      });
  const auto response = transport.request(
      "POST", "http://loopback", {{"X-Test", "1"}}, "{}", 100);
  if (!response.transport_ok || response.status != 201 || response.body != "{ok}" ||
      scheduled != 1) {
    std::printf("scheduled_http_transport_tests: FAIL\n");
    return 1;
  }

  ScheduledHttpTransport timeout(
      [](std::function<void()>) {},
      [](const std::string&, const std::string&,
         const std::vector<std::pair<std::string, std::string>>&, const std::string&, int64_t) {
        return HttpResponse{true, 200, "never"};
      });
  const auto expired = timeout.request("GET", "http://loopback", {}, "", 0);
  if (expired.transport_ok) {
    std::printf("scheduled_http_transport_tests: timeout FAIL\n");
    return 1;
  }
  std::printf("scheduled_http_transport_tests: PASS\n");
  return 0;
}
