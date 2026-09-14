#include <cstdint>
#include <cstdio>
#include <limits>

#include "time/time_authority.h"

using claw4::ports::ClockPort;
using claw4::time::ReconcileKind;
using claw4::time::SyncOutcome;
using claw4::time::TimeAuthority;
using claw4::time::TimeAuthorityConfig;
using claw4::time::TimeQuality;

class FakeClock final : public ClockPort {
 public:
  int64_t mono = 0;
  int epoch_reads = 0;
  int sync_reads = 0;
  int64_t epochSeconds() override { ++epoch_reads; return 777; }
  int64_t monotonicMs() override { return mono; }
  bool isTimeSynced() override { ++sync_reads; return true; }
};

static int cases = 0;
static int failures = 0;
#define CASE(fn) do { ++cases; if (!fn()) { ++failures; std::printf("FAIL %s\n", #fn); } } while (0)
#define CHECK(x) do { if (!(x)) { std::printf("  assert %s line %d\n", #x, __LINE__); return false; } } while (0)

static TimeAuthority make(FakeClock& c) {
  return TimeAuthority(c, "boot-a", TimeAuthorityConfig{1000, 3000, 3000, 100});
}

static bool unsynced_has_no_epoch() {
  FakeClock c; auto t = make(c); const auto s = t.status();
  CHECK(s.quality == TimeQuality::Unsynced); CHECK(!s.epoch_valid);
  CHECK(!s.epoch_ms.has_value()); CHECK(!s.age_ms.has_value());
  CHECK(c.epoch_reads == 0 && c.sync_reads == 0); return true;
}

static bool initial_sync_and_age() {
  FakeClock c; c.mono = 100; auto t = make(c);
  const auto r = t.acceptSync(1'700'000'000'000, "rtc", 20);
  CHECK(r.outcome == SyncOutcome::Accepted && r.reconcile == ReconcileKind::InitialAnchor);
  c.mono = 600; auto s = t.status();
  CHECK(s.quality == TimeQuality::Synced && s.epoch_ms == 1'700'000'000'500);
  CHECK(s.age_ms == 500 && s.last_sync_epoch_ms == 1'700'000'000'000);
  CHECK(s.last_sync_monotonic_ms == 100 && s.last_sync_source == "rtc");
  CHECK(s.uncertainty_ms == 21 && s.calendar_allowed); return true;
}

static bool holdover_then_stale_and_calendar_budget() {
  FakeClock c; auto t = make(c); CHECK(t.acceptSync(1000, "net", 100).outcome == SyncOutcome::Accepted);
  c.mono = 500; auto s = t.status(); CHECK(s.quality == TimeQuality::Synced);
  c.mono = 2200; s = t.status(); CHECK(s.quality == TimeQuality::Stale);
  CHECK(s.holdover_window && s.calendar_allowed);
  c.mono = 4001; s = t.status(); CHECK(s.quality == TimeQuality::Stale); CHECK(!s.calendar_allowed);
  return true;
}

static bool reconcile_forward_and_backward() {
  FakeClock c; auto t = make(c); CHECK(t.acceptSync(1000, "a", 0).outcome == SyncOutcome::Accepted);
  c.mono = 100; auto f = t.acceptSync(1200, "b", 0);
  CHECK(f.reconcile == ReconcileKind::ForwardJump && f.delta_ms == 100);
  c.mono = 200; auto b = t.acceptSync(1100, "c", 0);
  CHECK(b.reconcile == ReconcileKind::BackwardJump && b.delta_ms == -200); return true;
}

static bool monotonic_rollback_rejected() {
  FakeClock c; auto t = make(c); c.mono = 500; CHECK(t.acceptSync(1000, "a", 0).outcome == SyncOutcome::Accepted);
  c.mono = 499; auto r = t.acceptSync(1001, "b", 0); CHECK(r.outcome == SyncOutcome::RejectedMonotonicRollback);
  auto s = t.status(); CHECK(s.monotonic_regressed && !s.epoch_valid); return true;
}

static bool invalid_sync_inputs() {
  FakeClock c; auto t = make(c); CHECK(t.acceptSync(-1, "bad", 0).outcome == SyncOutcome::RejectedInvalidInput);
  CHECK(t.acceptSync(1, "bad", -1).outcome == SyncOutcome::RejectedInvalidInput);
  CHECK(t.status().quality == TimeQuality::Unsynced); return true;
}

static bool rollback_before_sync_recovers_with_anchor() {
  FakeClock c; auto t = make(c); c.mono = 500; (void)t.status();
  c.mono = 400; auto bad = t.status(); CHECK(bad.monotonic_regressed && !bad.epoch_valid);
  c.mono = 600; auto r = t.acceptSync(9000, "trusted", 1);
  CHECK(r.outcome == SyncOutcome::Accepted); CHECK(t.status().epoch_valid); return true;
}

static bool unknown_drift_disables_calendar() {
  FakeClock c; TimeAuthority t(c, "boot", TimeAuthorityConfig{1000, 3000, 60000, std::nullopt});
  CHECK(t.acceptSync(1000, "trusted", 10).outcome == SyncOutcome::Accepted);
  c.mono = 2000; auto s = t.status();
  CHECK(s.quality == TimeQuality::Stale && !s.uncertainty_ms.has_value());
  CHECK(s.holdover_window && !s.calendar_allowed); return true;
}

static bool four_hours_at_100ppm_rounds_up() {
  FakeClock c; TimeAuthority t(c, "boot", TimeAuthorityConfig{1000, 20000000, 5000, 100});
  CHECK(t.acceptSync(1000, "trusted", 17).outcome == SyncOutcome::Accepted);
  c.mono = 4 * 60 * 60 * 1000; auto s = t.status();
  CHECK(s.uncertainty_ms == 1457); // 17 ms + ceil(1,440 ms)
  CHECK(s.calendar_allowed); return true;
}

static bool drift_extremes_saturate_without_bad_rounding() {
  FakeClock c; TimeAuthority t(c, "boot", TimeAuthorityConfig{1000, std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max()});
  CHECK(t.acceptSync(1000, "trusted", 0).outcome == SyncOutcome::Accepted);
  c.mono = std::numeric_limits<int64_t>::max(); auto s = t.status(); CHECK(s.uncertainty_ms == std::numeric_limits<int64_t>::max());
  FakeClock d; TimeAuthority u(d, "boot", TimeAuthorityConfig{1000, 10000000, 5000, 1});
  CHECK(u.acceptSync(1000, "trusted", 0).outcome == SyncOutcome::Accepted);
  d.mono = 1; CHECK(u.status().uncertainty_ms == 1); // ceil(1*1/1e6)
  return true;
}

static bool empty_identity_rejected_without_state_change() {
  FakeClock c; TimeAuthority t(c, "", TimeAuthorityConfig{});
  CHECK(t.acceptSync(1000, "trusted", 0).outcome == SyncOutcome::RejectedInvalidInput);
  TimeAuthority good(c, "boot");
  CHECK(good.acceptSync(1000, "", 0).outcome == SyncOutcome::RejectedInvalidInput);
  CHECK(!good.status().epoch_valid); return true;
}

static bool restart_boot_isolation() {
  FakeClock c; auto old = make(c); c.mono = 9000; CHECK(old.acceptSync(5000, "a", 0).outcome == SyncOutcome::Accepted);
  c.mono = 0; TimeAuthority fresh(c, "boot-b"); auto s = fresh.status();
  CHECK(s.boot_id == "boot-b" && s.quality == TimeQuality::Unsynced && !s.epoch_valid); return true;
}

static bool overflow_is_safe() {
  FakeClock c; auto t = make(c);
  CHECK(t.acceptSync(std::numeric_limits<int64_t>::max() - 10, "a", 0).outcome == SyncOutcome::Accepted);
  c.mono = 20; auto s = t.status(); CHECK(!s.epoch_valid && !s.calendar_allowed); return true;
}

static bool exact_policy_boundaries_and_invalid_sync_preserve_anchor() {
  FakeClock c; TimeAuthority t(c, "boot", TimeAuthorityConfig{1000, 2000, 2, 1000});
  CHECK(t.acceptSync(86'399'500, "trusted", 0).outcome == SyncOutcome::Accepted);
  c.mono = 1000; auto s = t.status();
  CHECK(s.quality == TimeQuality::Synced && s.epoch_ms == 86'400'500);
  CHECK(t.acceptSync(1, "", 0).outcome == SyncOutcome::RejectedInvalidInput);
  CHECK(t.status().last_sync_source == "trusted");
  c.mono = 2000; s = t.status();
  CHECK(s.quality == TimeQuality::Stale && s.calendar_allowed && s.uncertainty_ms == 2);
  c.mono = 2001; CHECK(!t.status().calendar_allowed);
  return true;
}

int main() {
  CASE(unsynced_has_no_epoch); CASE(initial_sync_and_age);
  CASE(holdover_then_stale_and_calendar_budget); CASE(reconcile_forward_and_backward);
  CASE(monotonic_rollback_rejected); CASE(invalid_sync_inputs);
  CASE(rollback_before_sync_recovers_with_anchor);
  CASE(unknown_drift_disables_calendar); CASE(empty_identity_rejected_without_state_change);
  CASE(four_hours_at_100ppm_rounds_up);
  CASE(drift_extremes_saturate_without_bad_rounding);
  CASE(restart_boot_isolation); CASE(overflow_is_safe);
  CASE(exact_policy_boundaries_and_invalid_sync_preserve_anchor);
  std::printf("TimeAuthority: %d cases, %d failures\n", cases, failures);
  return failures == 0 ? 0 : 1;
}
