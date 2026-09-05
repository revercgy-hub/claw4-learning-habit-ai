// CODEX-APP-FIRST-001 AF1b — platform-neutral HTTP request boundary.
// The backend client below owns endpoint/JSON semantics; a device or host
// adapter owns sockets, TLS, headers and scheduling.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace claw4 {
namespace sync {

struct HttpResponse {
  bool transport_ok = false;
  int status = 0;
  std::string body;
};

class HttpTransport {
 public:
  virtual ~HttpTransport() = default;
  virtual HttpResponse request(const std::string& method,
                               const std::string& url,
                               const std::vector<std::pair<std::string, std::string>>& headers,
                               const std::string& body,
                               int64_t timeout_ms) = 0;
};

}  // namespace sync
}  // namespace claw4
