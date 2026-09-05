// CODEX-APP-FIRST-001 AF1b — Backend endpoint client over HttpTransport.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "sync/http_transport.h"
#include "sync/sync_client.h"
#include "sync/wire_codec.h"

namespace claw4 {
namespace sync {

class BackendClient {
 public:
  using ChallengeSigner = std::function<std::string(const std::string& device_id,
                                                    const std::string& challenge_id,
                                                    const std::string& nonce)>;

  BackendClient(HttpTransport& transport, std::string base_url,
                int64_t timeout_ms = 5000);

  bool authenticate(const std::string& device_id, const ChallengeSigner& signer,
                    wire::AuthResponse& auth, SyncErrorClass& error);
  bool fetchToday(const std::string& child_id, wire::TodayResponse& today,
                  SyncErrorClass& error);
  SyncClient::Response syncBatch(const SyncClient::Request& request);

  const std::string& accessToken() const { return access_token_; }
  void clearAuth() { access_token_.clear(); }

 private:
  HttpResponse Request(const std::string& method, const std::string& path,
                       const std::string& body, bool authenticated);
  static SyncErrorClass ClassifyHttp(int status);
  static std::string JoinUrl(const std::string& base, const std::string& path);

  HttpTransport& transport_;
  std::string base_url_;
  int64_t timeout_ms_;
  std::string access_token_;
};

}  // namespace sync
}  // namespace claw4
