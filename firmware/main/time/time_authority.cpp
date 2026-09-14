#include "time/time_authority.h"

#include <limits>
#include <utility>

namespace claw4::time {

TimeAuthority::TimeAuthority(ports::ClockPort& clock, std::string boot_id,
                             TimeAuthorityConfig config)
    : clock_(clock), boot_id_(std::move(boot_id)), config_(config) {
  if (config_.stale_after_ms < 0) config_.stale_after_ms = 0;
  if (config_.max_holdover_ms < config_.stale_after_ms)
    config_.max_holdover_ms = config_.stale_after_ms;
  if (config_.max_calendar_error_ms < 0) config_.max_calendar_error_ms = 0;
  if (config_.drift_upper_bound_ppm && *config_.drift_upper_bound_ppm < 0)
    config_.drift_upper_bound_ppm.reset();
}

int64_t TimeAuthority::saturatingAdd(const int64_t a, const int64_t b) {
  if (b > 0 && a > std::numeric_limits<int64_t>::max() - b)
    return std::numeric_limits<int64_t>::max();
  if (b < 0 && a < std::numeric_limits<int64_t>::min() - b)
    return std::numeric_limits<int64_t>::min();
  return a + b;
}

int64_t TimeAuthority::saturatingSub(const int64_t a, const int64_t b) {
  if (b == std::numeric_limits<int64_t>::min())
    return a >= 0 ? std::numeric_limits<int64_t>::max()
                  : saturatingAdd(saturatingAdd(a, std::numeric_limits<int64_t>::max()), 1);
  return saturatingAdd(a, -b);
}

bool TimeAuthority::checkedAdd(const int64_t a, const int64_t b, int64_t& out) {
  if ((b > 0 && a > std::numeric_limits<int64_t>::max() - b) ||
      (b < 0 && a < std::numeric_limits<int64_t>::min() - b)) return false;
  out = a + b;
  return true;
}

TimeStatus TimeAuthority::status() { return readStatus(); }

TimeStatus TimeAuthority::readStatus() {
  TimeStatus out;
  out.boot_id = boot_id_;
  const int64_t mono = clock_.monotonicMs();
  out.monotonic_ms = mono;
  if (mono < 0 || (have_mono_ && mono < last_mono_ms_)) {
    monotonic_regressed_ = true;
  } else {
    last_mono_ms_ = mono;
    have_mono_ = true;
  }
  out.monotonic_regressed = monotonic_regressed_;
  if (!have_sync_ || monotonic_regressed_ || mono < 0) return out;

  const int64_t age = saturatingSub(mono, sync_mono_ms_);
  out.age_ms = age;
  out.last_sync_epoch_ms = sync_epoch_ms_;
  out.last_sync_monotonic_ms = sync_mono_ms_;
  out.last_sync_source = sync_source_;
  if (config_.drift_upper_bound_ppm) {
    const auto ppm = *config_.drift_upper_bound_ppm;
#if defined(__SIZEOF_INT128__)
    const __int128 wide = static_cast<__int128>(age) * ppm;
    const __int128 rounded = (wide + 999999) / 1000000;
    const int64_t drift = rounded > std::numeric_limits<int64_t>::max()
                              ? std::numeric_limits<int64_t>::max()
                              : static_cast<int64_t>(rounded);
#else
    const int64_t drift = saturatingAdd(age / 1000000 * ppm,
                                        (age % 1000000 && ppm) ? ppm : 0);
#endif
    out.uncertainty_ms = saturatingAdd(uncertainty_ms_, drift);
  }
  int64_t epoch = 0;
  out.epoch_valid = checkedAdd(sync_epoch_ms_, age, epoch);
  if (out.epoch_valid) out.epoch_ms = epoch;
  if (age <= config_.stale_after_ms) {
    out.quality = TimeQuality::Synced;
  } else {
    out.quality = TimeQuality::Stale;
  }
  out.holdover_window = out.quality != TimeQuality::Unsynced &&
                        age <= config_.max_holdover_ms;
  out.calendar_allowed = out.epoch_valid && out.holdover_window &&
      out.uncertainty_ms.has_value() &&
      *out.uncertainty_ms <= config_.max_calendar_error_ms;
  return out;
}

SyncResult TimeAuthority::acceptSync(const int64_t epoch_ms, std::string source,
                                     const int64_t uncertainty_ms) {
  SyncResult result;
  result.source = source;
  if (epoch_ms < 0 || uncertainty_ms < 0 || boot_id_.empty() || source.empty()) return result;
  const int64_t mono = clock_.monotonicMs();
  if (mono < 0 || (have_mono_ && mono < last_mono_ms_)) {
    monotonic_regressed_ = true;
    result.outcome = SyncOutcome::RejectedMonotonicRollback;
    return result;
  }
  last_mono_ms_ = mono;
  have_mono_ = true;
  const int64_t predicted = have_sync_ ? saturatingAdd(sync_epoch_ms_,
                                                       saturatingSub(mono, sync_mono_ms_))
                                       : epoch_ms;
  result.delta_ms = saturatingSub(epoch_ms, predicted);
  result.reconcile = have_sync_ ? (result.delta_ms < 0 ? ReconcileKind::BackwardJump
                                                        : result.delta_ms > 0
                                                            ? ReconcileKind::ForwardJump
                                                            : ReconcileKind::None)
                                : ReconcileKind::InitialAnchor;
  have_sync_ = true;
  monotonic_regressed_ = false;
  sync_epoch_ms_ = epoch_ms;
  sync_mono_ms_ = mono;
  uncertainty_ms_ = uncertainty_ms;
  sync_source_ = std::move(source);
  result.outcome = SyncOutcome::Accepted;
  return result;
}

}  // namespace claw4::time
