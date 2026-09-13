#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ports/clock_port.h"

namespace claw4::time {

enum class TimeQuality { Unsynced, Synced, Stale, Holdover };
enum class SyncOutcome { Accepted, RejectedInvalidInput, RejectedMonotonicRollback };
enum class ReconcileKind { None, ForwardJump, BackwardJump, InitialAnchor };

struct TimeAuthorityConfig {
  // These are policy defaults, not measured hardware guarantees.
  int64_t stale_after_ms = 60 * 60 * 1000;
  int64_t max_holdover_ms = 24 * 60 * 60 * 1000;
  int64_t max_calendar_error_ms = 60 * 1000;
};

struct TimeStatus {
  std::string boot_id;
  int64_t monotonic_ms = 0;
  TimeQuality quality = TimeQuality::Unsynced;
  bool epoch_valid = false;
  std::optional<int64_t> epoch_ms;
  std::optional<int64_t> age_ms;
  std::optional<int64_t> last_sync_epoch_ms;
  std::optional<int64_t> last_sync_monotonic_ms;
  std::string last_sync_source;
  int64_t uncertainty_ms = 0;
  bool calendar_allowed = false;
  bool monotonic_regressed = false;
};

struct SyncResult {
  SyncOutcome outcome = SyncOutcome::RejectedInvalidInput;
  ReconcileKind reconcile = ReconcileKind::None;
  int64_t delta_ms = 0;
  std::string source;
};

// Host policy layer. It deliberately does not read ClockPort::epochSeconds()
// or isTimeSynced(): those values may be uptime/placeholders on a device.
class TimeAuthority {
 public:
  TimeAuthority(ports::ClockPort& clock, std::string boot_id,
                TimeAuthorityConfig config = {});

  TimeStatus status();
  SyncResult acceptSync(int64_t epoch_ms, std::string source,
                        int64_t uncertainty_ms);

 private:
  static int64_t saturatingAdd(int64_t a, int64_t b);
  static int64_t saturatingSub(int64_t a, int64_t b);
  TimeStatus readStatus();

  ports::ClockPort& clock_;
  std::string boot_id_;
  TimeAuthorityConfig config_;
  bool have_sync_ = false;
  bool monotonic_regressed_ = false;
  int64_t last_mono_ms_ = 0;
  int64_t sync_epoch_ms_ = 0;
  int64_t sync_mono_ms_ = 0;
  int64_t uncertainty_ms_ = 0;
  std::string sync_source_;
};

}  // namespace claw4::time
