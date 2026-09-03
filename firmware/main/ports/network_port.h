// claw4/firmware/main/ports/network_port.h
// NetworkPort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// MVP boundary only: connectivity state + a single JSON POST. The real HTTP /
// TLS / WiFi stack is a later device-network task; the P17 adapter may extend
// this interface without touching the learning domain. Portable C++17.
#pragma once

#include <cstdint>
#include <string>

namespace claw4 {
namespace ports {

struct NetworkStatus {
  bool online = false;
  std::string ssid;
};

class NetworkPort {
 public:
  virtual ~NetworkPort() = default;
  virtual NetworkStatus status() = 0;
  // MVP sync path: POST a JSON body. Returns true and fills `response` on
  // success; false on any transport/timeout failure.
  virtual bool postJson(const std::string& url, const std::string& body,
                        int64_t timeout_ms, std::string& response) = 0;
};

}  // namespace ports
}  // namespace claw4
