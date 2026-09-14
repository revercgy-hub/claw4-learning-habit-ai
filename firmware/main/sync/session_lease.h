// claw4/firmware/main/sync/session_lease.h
// WB-V53-NEXT-001 REVIEW-FIX-001 RF1: backend-session lease + generation
// ownership, extracted into a pure C++ helper so the DEVICE runtime and the
// HOST gate enforce exactly the same rule.
//
// Why a lease and not just a generation: a coordinator generation is bumped on
// every reconfigure, but a session may also be superseded by a NEW session that
// happens to carry the same generation value (e.g. two quick reconfigures).
// `lease_id` is a monotonically increasing counter owned by the runtime, so
// exactly one session instance is ever "current".
//
// Portable C++17. No LVGL / ESP-IDF / FreeRTOS / BSP dependencies.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>

namespace claw4 {
namespace sync {

struct SessionLease {
  int64_t lease_id = 0;    // monotonically increasing per runtime, never reused
  int64_t generation = 0;  // coordinator generation captured on acquisition

  bool matches(const SessionLease& live) const {
    return lease_id == live.lease_id && generation == live.generation;
  }
};

// Implemented by whoever owns the current session: the device runtime
// (LearningRuntime) and the host gate fake.
class SessionLeaseSource {
 public:
  virtual ~SessionLeaseSource() = default;
  virtual SessionLease currentLease() const = 0;
  virtual bool isCurrent(const SessionLease& lease) const {
    return lease.matches(currentLease());
  }
};

// A source that never supersedes anything. Used by the legacy single-session
// Host tests that build a session directly.
class AlwaysCurrentLeaseSource final : public SessionLeaseSource {
 public:
  explicit AlwaysCurrentLeaseSource(SessionLease lease) : lease_(lease) {}
  SessionLease currentLease() const override { return lease_; }

 private:
  SessionLease lease_;
};

// RF1: the production owner of the current session. It is a TEMPLATE so the
// device runtime and the Host gate instantiate the SAME code — the device
// path must not have its own untested copy.
//
// Two guarantees it provides:
//   * borrow() returns a shared_ptr copy, so the network phase keeps the
//     session object alive even if install() replaces it concurrently
//     (no use-after-free on the backend object);
//   * install() bumps lease_id, so exactly one session instance is ever
//     "current" and every older lease is refused by isCurrent()/accept().
template <typename SessionT>
class SessionLeaseHolder final : public SessionLeaseSource {
 public:
  // Swaps in a new session and returns the lease it was installed with.
  SessionLease install(std::shared_ptr<SessionT> session, int64_t generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++lease_counter_;
    current_ = std::move(session);
    lease_ = SessionLease{lease_counter_, generation};
    return lease_;
  }

  // Reserves the next lease and builds the session with it, so the session is
  // constructed already bound to its own lease (no window where it observes a
  // stale/zero lease).
  template <typename Factory>
  SessionLease installWith(int64_t generation, Factory make) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++lease_counter_;
    lease_ = SessionLease{lease_counter_, generation};
    current_ = make(lease_);
    return lease_;
  }

  // Lease the current session for a whole network phase.
  std::shared_ptr<SessionT> borrow() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
  }

  SessionLease currentLease() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return lease_;
  }

  // True only while `session` is still the installed instance.
  bool isInstalled(const SessionT* session) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_.get() == session;
  }

  // The publish rule for immutable snapshots: a result may only be published by
  // the session that is STILL installed and whose lease is still current.
  bool accepts(const SessionT* session, const SessionLease& lease) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_.get() == session && lease.matches(lease_);
  }

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<SessionT> current_;
  SessionLease lease_;
  int64_t lease_counter_ = 0;
};

}  // namespace sync
}  // namespace claw4
