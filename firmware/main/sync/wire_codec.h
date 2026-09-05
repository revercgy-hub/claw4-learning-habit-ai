// CODEX-APP-FIRST-001 AF1 — target-portable JSON wire codec.
//
// The codec owns request/response shape for Auth, Today and Events/ACK. It
// intentionally has no HTTP, TLS, IDF, or Python dependency. The device and
// Virtual Device runner can share it; a transport only moves the resulting
// UTF-8 JSON bytes.
#pragma once

#include <string>
#include <vector>

#include "learning_domain/task.h"
#include "sync/sync_client.h"

namespace claw4 {
namespace sync {
namespace wire {

struct ChallengeRequest {
  std::string device_id;
};

struct ChallengeResponse {
  std::string challenge_id;
  std::string nonce;
  int64_t expires_at = 0;
};

struct AuthRequest {
  std::string device_id;
  std::string challenge_id;
  std::string nonce;
  std::string challenge_signature;
};

struct AuthResponse {
  std::string access_token;
  int64_t expires_in = 0;
};

struct TodayRequest {
  std::string child_id;
};

struct TodayResponse {
  std::string date;
  std::vector<claw4::domain::Task> tasks;
};

std::string EncodeChallengeRequest(const ChallengeRequest& request);
bool DecodeChallengeResponse(const std::string& json,
                             ChallengeResponse& response);

std::string EncodeAuthRequest(const AuthRequest& request);
bool DecodeAuthResponse(const std::string& json, AuthResponse& response);

std::string EncodeTodayRequest(const TodayRequest& request);
bool DecodeTodayResponse(const std::string& json, TodayResponse& response);

std::string EncodeBatchRequest(const SyncClient::Request& request);
bool DecodeBatchResponse(const std::string& json, SyncClient::Response& response);

}  // namespace wire
}  // namespace sync
}  // namespace claw4
