// claw4/firmware/main/telemetry/logger.h
// Redacted structured logging interface.
// Portable C++17. MUST NOT accept or log tokens, secrets, child names or
// content data (ARCHITECTURE.md §10.2, 项目总规划 §二十八).
#pragma once

#include <cstdint>
#include <string_view>

namespace claw4 {
namespace telemetry {

// Structured log tags (ARCHITECTURE.md §10.2).
enum class LogTag : uint8_t {
  Device = 0,
  Network,
  Sync,
  Task,
  Power,
  Error,
};

// Redacted structured logger. Implementations write only to local structured
// logs in MVP; telemetry upload is V1 (ARCHITECTURE.md §3.7 / 项目总规划 §二十九).
//
// FORBIDDEN FIELDS (must never be passed to this interface): API keys, tokens,
// device secrets, nonces, challenge signatures, SSID/passwords, child names,
// photos, location, full audio, other device credentials.
class Logger {
 public:
  virtual ~Logger() = default;

  virtual void log(LogTag tag, std::string_view message) = 0;
  virtual void logMetric(std::string_view name, int64_t value) = 0;
};

}  // namespace telemetry
}  // namespace claw4
