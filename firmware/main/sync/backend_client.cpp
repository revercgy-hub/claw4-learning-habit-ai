// CODEX-APP-FIRST-001 AF1b — Backend endpoint client implementation.
#include "sync/backend_client.h"

#include <utility>

namespace claw4 {
namespace sync {

BackendClient::BackendClient(HttpTransport& transport, std::string base_url,
                             int64_t timeout_ms)
    : transport_(transport), base_url_(std::move(base_url)), timeout_ms_(timeout_ms) {}

std::string BackendClient::JoinUrl(const std::string& base,
                                   const std::string& path) {
  if (base.empty()) return path;
  if (base.back() == '/' && !path.empty() && path.front() == '/') {
    return base.substr(0, base.size() - 1) + path;
  }
  if (base.back() != '/' && (path.empty() || path.front() != '/')) {
    return base + "/" + path;
  }
  return base + path;
}

SyncErrorClass BackendClient::ClassifyHttp(const int status) {
  if (status == 401 || status == 403) return SyncErrorClass::Auth;
  if (status >= 500 && status <= 599) return SyncErrorClass::Server;
  if (status >= 400 && status <= 499) return SyncErrorClass::Business;
  if (status >= 200 && status <= 299) return SyncErrorClass::None;
  return SyncErrorClass::Unknown;
}

HttpResponse BackendClient::Request(const std::string& method,
                                    const std::string& path,
                                    const std::string& body,
                                    const bool authenticated) {
  std::vector<std::pair<std::string, std::string>> headers;
  headers.emplace_back("Content-Type", "application/json");
  if (authenticated && !access_token_.empty()) {
    headers.emplace_back("Authorization", "Bearer " + access_token_);
  }
  return transport_.request(method, JoinUrl(base_url_, path), headers, body,
                            timeout_ms_);
}

bool BackendClient::authenticate(const std::string& device_id,
                                 const ChallengeSigner& signer,
                                 wire::AuthResponse& auth,
                                 SyncErrorClass& error) {
  error = SyncErrorClass::None;
  wire::ChallengeResponse challenge;
  const auto challenge_response = Request(
      "POST", "/api/v1/devices/challenge",
      wire::EncodeChallengeRequest({device_id}), false);
  if (!challenge_response.transport_ok) {
    error = SyncErrorClass::Network;
    return false;
  }
  error = ClassifyHttp(challenge_response.status);
  if (error != SyncErrorClass::None ||
      !wire::DecodeChallengeResponse(challenge_response.body, challenge)) {
    if (error == SyncErrorClass::None) error = SyncErrorClass::Unknown;
    return false;
  }

  wire::AuthRequest request;
  request.device_id = device_id;
  request.challenge_id = challenge.challenge_id;
  request.nonce = challenge.nonce;
  request.challenge_signature = signer(device_id, challenge.challenge_id,
                                       challenge.nonce);
  const auto auth_response = Request("POST", "/api/v1/devices/auth",
                                     wire::EncodeAuthRequest(request), false);
  if (!auth_response.transport_ok) {
    error = SyncErrorClass::Network;
    return false;
  }
  error = ClassifyHttp(auth_response.status);
  if (error != SyncErrorClass::None ||
      !wire::DecodeAuthResponse(auth_response.body, auth)) {
    if (error == SyncErrorClass::None) error = SyncErrorClass::Unknown;
    return false;
  }
  access_token_ = auth.access_token;
  return true;
}

bool BackendClient::fetchToday(const std::string& child_id,
                               wire::TodayResponse& today,
                               SyncErrorClass& error) {
  error = SyncErrorClass::None;
  const auto response = Request("GET", "/api/v1/children/" + child_id +
                                        "/tasks/today", "", true);
  if (!response.transport_ok) {
    error = SyncErrorClass::Network;
    return false;
  }
  error = ClassifyHttp(response.status);
  if (error != SyncErrorClass::None ||
      !wire::DecodeTodayResponse(response.body, today)) {
    if (error == SyncErrorClass::None) error = SyncErrorClass::Unknown;
    return false;
  }
  return true;
}

SyncClient::Response BackendClient::syncBatch(const SyncClient::Request& request) {
  SyncClient::Response result;
  const auto response = Request("POST", "/api/v1/events/batch",
                                wire::EncodeBatchRequest(request), true);
  if (!response.transport_ok) {
    result.error_class = SyncErrorClass::Network;
    return result;
  }
  result.http_status = response.status;
  result.error_class = ClassifyHttp(response.status);
  if (result.error_class != SyncErrorClass::None ||
      !wire::DecodeBatchResponse(response.body, result)) {
    if (result.error_class == SyncErrorClass::None) {
      result.error_class = SyncErrorClass::Unknown;
    }
    return result;
  }
  result.http_status = response.status;
  return result;
}

}  // namespace sync
}  // namespace claw4
