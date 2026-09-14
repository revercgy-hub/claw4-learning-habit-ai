// claw4/firmware/tests/unit/v53/final_concurrency_cleanup_tests.cpp
// WB-V53-NEXT-001 FINAL-CONCURRENCY-CLEANUP — FIX-1 + FIX-2.
//
// FIX-1: diagnostics ownership check + publish must be ONE state transaction.
//   Cases 1-2 drive sync::publishSessionSnapshot() — the SAME helper the device
//   runtime calls from LearningRuntime::RunOnlineCycle() — with an instrumented
//   lease source so the test can prove the check really happens INSIDE the
//   state critical section (and that the pre-fix shape would be detected).
//
// FIX-2: the lock-injected cycle must never read Coordinator/storage counters
//   outside the state lock.
//   pendingCount()/lastAcked() both go through OutboxStorage::load(), so a
//   delegating storage that records whether the state lock is held at every
//   entry point is a faithful probe: a counter read that escapes the
//   transaction shows up as `ops_unlocked > 0`.
//
// Deterministic: latches / condition_variable only, never sleeps.
// Only the socket layer is stubbed (same method/url/body/status contract);
// BackendClient, AppCoordinator, SyncExecutor, SessionLeaseHolder and the
// publisher helper are the real production code.

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "application/coordinator.h"
#include "learning_domain/reducer.h"
#include "sync/http_transport.h"
#include "sync/learning_backend_session.h"
#include "sync/outbox_storage.h"
#include "sync/session_lease.h"
#include "sync/session_snapshot_publisher.h"
#include "fakes/fake_outbox_storage.h"

using namespace claw4::application;
using namespace claw4::domain;
using namespace claw4::sync;

static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                             \
  do {                                         \
    ++g_cases;                                 \
    if (!run_case_##name()) {                  \
      std::printf("FAIL: %s\n", #name);        \
      ++g_fail;                                \
    }                                          \
  } while (0)

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      std::printf("  assert fail: %s (line %d)\n", #cond, __LINE__);  \
      return false;                                                   \
    }                                                                 \
  } while (0)

namespace {

using SessionT = LearningBackendSession;
const char* kUrl = "http://127.0.0.1:8000";  // socket is stubbed
const char* kChallenge =
    "{\"challenge_id\":\"ch-1\",\"nonce\":\"n-1\",\"expires_at\":1700000100}";
const char* kAuth = "{\"access_token\":\"tok-1\",\"expires_in\":3600}";

HttpResponse rsp(const int status, const std::string& body) {
  HttpResponse r;
  r.transport_ok = true;
  r.status = status;
  r.body = body;
  return r;
}

std::string todayBody(const char* task_id, const char* status, const int version) {
  return std::string("{\"date\":\"2026-09-06\",\"tasks\":[{\"task_id\":\"") +
         task_id + "\",\"title\":\"Math\",\"subject\":\"math\"," +
         "\"estimated_minutes\":20,\"priority\":\"high\",\"status\":\"" + status +
         "\",\"scheduled_date\":\"2026-09-06\",\"version\":" +
         std::to_string(version) + "}]}";
}

std::string batchBody(const int64_t last_acked,
                      const std::vector<std::string>& event_ids,
                      const char* outcome) {
  std::string token(outcome);
  for (auto& c : token) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  std::string results;
  int64_t seq = 1;
  for (const auto& id : event_ids) {
    if (!results.empty()) results += ",";
    results += std::string("{\"event_id\":\"") + id + "\",\"sequence\":" +
               std::to_string(seq) + ",\"status\":\"" + token +
               "\",\"http_status\":200}";
    ++seq;
  }
  return std::string("{\"last_acked_sequence\":") + std::to_string(last_acked) +
         ",\"server_time\":1700000001,\"results\":[" + results + "]}";
}

std::string sign(const std::string& device, const std::string& challenge,
                 const std::string& nonce) {
  return device + ":" + challenge + ":" + nonce;
}

// ---------------------------------------------------------------------------
// probes
// ---------------------------------------------------------------------------

// Tracks whether the injected state lock is currently HELD. Only the owning
// thread mutates the depth; `held()` is a plain atomic read so another thread
// (or a test) can observe it without touching the mutex.
class LockDepth {
 public:
  void lock() {
    mutex_.lock();
    ++depth_;
  }
  void unlock() {
    --depth_;
    mutex_.unlock();
  }
  bool held() const { return depth_.load() > 0; }

 private:
  std::mutex mutex_;
  std::atomic<int> depth_{0};
};

struct DepthGuard {
  explicit DepthGuard(LockDepth& depth) : depth_(depth) { depth_.lock(); }
  ~DepthGuard() { depth_.unlock(); }
  DepthGuard(const DepthGuard&) = delete;
  DepthGuard& operator=(const DepthGuard&) = delete;
  LockDepth& depth_;
};

// Delegating storage that records, for EVERY entry point, whether the state
// lock was held. A counter read that escapes the state transaction shows up as
// ops_unlocked > 0.
class ProbeStorage final : public OutboxStorage {
 public:
  explicit ProbeStorage(std::shared_ptr<FakeDisk> disk)
      : inner_(std::move(disk)) {}

  LockDepth* lock_depth = nullptr;
  bool armed = false;
  int ops_locked = 0;
  int ops_unlocked = 0;

  bool load(OutboxState& out) override {
    Probe();
    return inner_.load(out);
  }
  CommitStatus commit(const DomainState& next_domain,
                      const std::vector<PendingEvent>& appended,
                      int64_t next_sequence) override {
    Probe();
    return inner_.commit(next_domain, appended, next_sequence);
  }
  CommitStatus commitDiagnostic(bool sync_failed, bool sync_recovered) override {
    Probe();
    return inner_.commitDiagnostic(sync_failed, sync_recovered);
  }
  CommitStatus removeAcked(int64_t up_to_sequence) override {
    Probe();
    return inner_.removeAcked(up_to_sequence);
  }
  CommitStatus markDeadLetter(const claw4::domain::EventId& event_id,
                              const std::string& reason) override {
    Probe();
    return inner_.markDeadLetter(event_id, reason);
  }

 private:
  void Probe() {
    if (!armed || lock_depth == nullptr) return;
    if (lock_depth->held()) {
      ++ops_locked;
    } else {
      ++ops_unlocked;
    }
  }
  FakeOutboxStorage inner_;
};

// The production SessionLeaseHolder plus a probe on accepts(). It stays a
// SessionLeaseSource so the session under test uses it exactly as the device
// uses the real holder.
template <typename SessionT>
class ProbedLeaseHolder final : public SessionLeaseSource {
 public:
  LockDepth* lock_depth = nullptr;
  std::function<void()> on_accepts;
  mutable bool armed = false;
  mutable bool probed = false;
  mutable bool probed_while_locked = false;

  std::shared_ptr<SessionT> borrow() const { return inner_.borrow(); }
  bool isInstalled(const SessionT* session) const {
    return inner_.isInstalled(session);
  }
  template <typename Factory>
  SessionLease installWith(int64_t generation, Factory make) {
    return inner_.installWith(generation, make);
  }
  SessionLease currentLease() const override { return inner_.currentLease(); }

  bool accepts(const SessionT* session, const SessionLease& lease) const {
    if (armed && !probed) {
      probed = true;
      probed_while_locked = lock_depth != nullptr && lock_depth->held();
    }
    if (on_accepts) on_accepts();
    return inner_.accepts(session, lease);
  }

 private:
  SessionLeaseHolder<SessionT> inner_;
};

class ScriptedSocket final : public HttpTransport {
 public:
  std::function<HttpResponse(const std::string& method, const std::string& url,
                             const std::string& body)>
      handler;
  LockDepth* lock_depth = nullptr;
  int challenge_calls = 0;
  int auth_calls = 0;
  int today_calls = 0;
  int events_calls = 0;
  int net_calls = 0;
  int net_calls_while_locked = 0;

  HttpResponse request(const std::string& method, const std::string& url,
                       const std::vector<std::pair<std::string, std::string>>&,
                       const std::string& body, int64_t) override {
    ++net_calls;
    if (lock_depth != nullptr && lock_depth->held()) ++net_calls_while_locked;
    if (url.find("challenge") != std::string::npos) ++challenge_calls;
    if (url.find("/devices/auth") != std::string::npos) ++auth_calls;
    if (url.find("today") != std::string::npos) ++today_calls;
    if (url.find("events/batch") != std::string::npos) ++events_calls;
    if (!handler) return HttpResponse{};
    return handler(method, url, body);
  }
};

struct Latch {
  std::mutex mutex;
  std::condition_variable cv;
  bool entered = false;
  bool released = false;

  void arriveAndWait() {
    std::unique_lock<std::mutex> lock(mutex);
    entered = true;
    cv.notify_all();
    cv.wait(lock, [this] { return released; });
  }
  void waitUntilEntered() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return entered; });
  }
  void release() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      released = true;
    }
    cv.notify_all();
  }
  ~Latch() { release(); }
};

struct WorkerJoiner {
  std::thread worker;
  Latch* latch = nullptr;
  ~WorkerJoiner() {
    if (latch != nullptr) latch->release();
    if (worker.joinable()) worker.join();
  }
};

// ---------------------------------------------------------------------------
// fixture: production wiring, reconfigure under the state lock (like
// LearningRuntime::ConfigureBackend)
// ---------------------------------------------------------------------------
struct Fixture {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  ProbeStorage storage{disk};
  DomainReducer reducer;
  AppCoordinator coord{storage, reducer};
  ProbedLeaseHolder<SessionT> holder;
  LockDepth lock_depth;
  ScriptedSocket* sock = nullptr;
  int64_t e = 0;

  Fixture() {
    storage.lock_depth = &lock_depth;
    holder.lock_depth = &lock_depth;
  }

  StateLockFn lockFn() { return [this] { lock_depth.lock(); }; }
  StateUnlockFn unlockFn() { return [this] { lock_depth.unlock(); }; }

  // Device-shaped install: the state lock is held for the whole
  // generation-bump + lease-install + session construction.
  SessionLease install() {
    DepthGuard guard(lock_depth);
    const int64_t generation = coord.beginNewSession();
    return holder.installWith(generation, [this](const SessionLease& lease) {
      auto transport = std::make_unique<ScriptedSocket>();
      sock = transport.get();
      sock->lock_depth = &lock_depth;
      return std::make_shared<SessionT>(
          coord, std::move(transport), kUrl, DeviceId{"dev-1"},
          ChildId{"child-1"}, &sign, [] { return int64_t{1700000001}; }, lease,
          &holder);
    });
  }

  ReducerContext ctx() {
    ReducerContext c;
    c.device_id = DeviceId{"dev-1"};
    c.child_id = ChildId{"child-1"};
    c.now_epoch = 1700000001;
    c.monotonic_ms = 1'000'000;
    c.make_event_id = [this] { return EventId{"ev-" + std::to_string(++e)}; };
    c.make_session_id = [this] { return SessionId{"sess-1"}; };
    return c;
  }

  bool seedTask() {
    Task t;
    t.task_id = TaskId{"task-1"};
    t.child_id = ChildId{"child-1"};
    t.title = "Math";
    t.status = TaskStatus::Ready;
    t.version = 3;
    return coord.applyTodaySnapshot({t});
  }
  bool startTask() {
    IntentRequest r;
    r.intent = Intent::StartTask;
    r.task_id = TaskId{"task-1"};
    return coord.dispatchIntent(r, ctx()).ok;
  }

  // Arms the counter probe and zeros its counters.
  void armProbe() {
    storage.armed = true;
    storage.ops_locked = 0;
    storage.ops_unlocked = 0;
  }

  void happyHandler(const int64_t ack_upto,
                    const std::vector<std::string>& ids) {
    sock->handler = [ack_upto, ids](const std::string&, const std::string& url,
                                    const std::string&) {
      if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
      if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
      if (url.find("today") != std::string::npos)
        return rsp(200, todayBody("task-1", "in_progress", 3));
      if (url.find("events/batch") != std::string::npos)
        return rsp(200, batchBody(ack_upto, ids, "Accepted"));
      return rsp(404, "");
    };
  }
};

}  // namespace

// ===========================================================================
// FIX-1
// ===========================================================================

// The ownership check must run INSIDE the state critical section. The probe
// records whether the injected lock was held when `accepts()` was asked; the
// pre-fix shape (check first, lock afterwards) is shown to be detected as
// UNLOCKED, so the assertion is not vacuous.
static bool run_case_diagnostics_publish_is_atomic_with_ownership_check() {
  Fixture f;
  f.install();
  const std::shared_ptr<SessionT> session = f.holder.borrow();
  CHECK(session != nullptr);

  BackendSessionDiagnostics snapshot;
  snapshot.last_operation = "produced-by-current-session";
  snapshot.pending_count = 7;
  BackendSessionDiagnostics slot;
  slot.last_operation = "placeholder";

  f.holder.armed = true;
  CHECK(publishSessionSnapshot(f.holder, *session, f.lockFn(), f.unlockFn(),
                               slot, snapshot));
  CHECK(f.holder.probed);
  CHECK(f.holder.probed_while_locked);  // <-- the discriminating assertion
  CHECK(slot.last_operation == "produced-by-current-session");
  CHECK(slot.pending_count == 7);
  CHECK(!f.lock_depth.held());  // the helper released the lock again

  // Probe sanity: replay the PRE-FIX shape (check outside the lock, write
  // inside) and confirm the probe reports "not locked". Without this the
  // assertion above could pass for the wrong reason.
  f.holder.armed = true;
  f.holder.probed = false;
  f.holder.probed_while_locked = false;
  BackendSessionDiagnostics old_slot;
  old_slot.last_operation = "placeholder";
  const bool old_shape_owned = f.holder.accepts(session.get(), session->lease());
  if (old_shape_owned) {
    DepthGuard guard(f.lock_depth);
    old_slot = snapshot;  // the pre-fix ordering: write long after the check
  }
  CHECK(old_shape_owned);                  // the session IS current...
  CHECK(!f.holder.probed_while_locked);    // ...but the check saw no lock
  CHECK(old_slot.last_operation == "produced-by-current-session");
  return true;
}

// The full scenario from the review: old cycle finished -> pause before the
// diagnostics publish -> reconfigure -> resume. Because the check and the write
// are one transaction, a reconfigure cannot land between them (proved by the
// latch), and once the reconfigure HAS happened the dead session can never
// publish again.
static bool run_case_stale_diagnostics_cannot_publish_after_reconfigure_between_check_and_write() {
  Fixture f;
  f.install();
  const std::shared_ptr<SessionT> old_session = f.holder.borrow();
  CHECK(old_session != nullptr);
  const SessionLease old_lease = old_session->lease();

  BackendSessionDiagnostics stale;
  stale.last_operation = "old-session-cycle";
  stale.pending_count = 99;

  // (1) Pause exactly INSIDE the ownership check, i.e. inside the state
  //     critical section. `held()` is read from another thread, so no
  //     try_lock-on-self-mutex trickery is needed.
  Latch inside_check;
  f.holder.on_accepts = [&inside_check] { inside_check.arriveAndWait(); };

  BackendSessionDiagnostics slot;
  slot.last_operation = "unset";
  bool published = false;
  bool lock_held_during_check = false;
  WorkerJoiner joiner;
  joiner.latch = &inside_check;
  joiner.worker = std::thread([&] {
    published = publishSessionSnapshot(f.holder, *old_session, f.lockFn(),
                                       f.unlockFn(), slot, stale);
  });
  inside_check.waitUntilEntered();
  lock_held_during_check = f.lock_depth.held();
  inside_check.release();
  joiner.worker.join();

  CHECK(lock_held_during_check);  // no reconfigure can interleave here
  CHECK(published);               // ...and the old session was still current
  CHECK(slot.last_operation == "old-session-cycle");

  // (2) NOW a reconfigure lands, and the new session publishes its own
  //     snapshot.
  f.holder.on_accepts = nullptr;
  const SessionLease fresh = f.install();
  const std::shared_ptr<SessionT> new_session = f.holder.borrow();
  CHECK(new_session != nullptr);
  CHECK(new_session.get() != old_session.get());
  CHECK(fresh.lease_id != old_lease.lease_id);

  BackendSessionDiagnostics fresh_snapshot;
  fresh_snapshot.last_operation = "new-session-cycle";
  CHECK(publishSessionSnapshot(f.holder, *new_session, f.lockFn(), f.unlockFn(),
                               slot, fresh_snapshot));
  CHECK(slot.last_operation == "new-session-cycle");

  // (3) The dead session's late publish is refused and the slot keeps the new
  //     session's value.
  CHECK(!publishSessionSnapshot(f.holder, *old_session, f.lockFn(),
                                f.unlockFn(), slot, stale));
  CHECK(slot.last_operation == "new-session-cycle");  // NOT overwritten
  CHECK(slot.pending_count != 99);
  CHECK(old_session->superseded());
  CHECK(f.holder.isInstalled(new_session.get()));
  return true;
}

// ===========================================================================
// FIX-2
// ===========================================================================

// 1) initial authenticate + /today, with no pending events
static bool run_case_probe_cycle_initial_auth_and_today_reads_counters_locked() {
  Fixture f;
  CHECK(f.seedTask());
  f.install();
  const std::shared_ptr<SessionT> session = f.holder.borrow();
  CHECK(session != nullptr);
  f.happyHandler(0, {});

  f.armProbe();
  CHECK(session->runOnlineCycle(f.lockFn(), f.unlockFn()));

  CHECK(f.sock->challenge_calls == 1);
  CHECK(f.sock->today_calls == 1);
  CHECK(f.storage.ops_unlocked == 0);  // NO counter read outside the lock
  CHECK(f.storage.ops_locked > 0);     // ...and the reads really happened
  CHECK(f.sock->net_calls_while_locked == 0);  // network stays lock-free
  CHECK(f.sock->net_calls > 0);
  return true;
}

// 2) events ACK path
static bool run_case_probe_cycle_events_ack_reads_counters_locked() {
  Fixture f;
  CHECK(f.seedTask());
  f.install();
  const std::shared_ptr<SessionT> session = f.holder.borrow();
  CHECK(session != nullptr);
  CHECK(f.startTask());
  CHECK(f.coord.pendingCount() == 2);
  f.happyHandler(2, {"ev-1", "ev-2"});

  f.armProbe();
  CHECK(session->runOnlineCycle(f.lockFn(), f.unlockFn()));

  CHECK(f.sock->events_calls == 1);
  CHECK(f.storage.ops_unlocked == 0);
  CHECK(f.storage.ops_locked > 0);
  CHECK(f.sock->net_calls_while_locked == 0);
  return true;
}

// 3) 401 -> credential refresh -> retry. This is the strongest discriminator:
//    the pre-fix code refreshed counters from inside authenticate(), which runs
//    with NO lock held.
static bool run_case_probe_cycle_401_reauth_reads_counters_locked() {
  Fixture f;
  CHECK(f.seedTask());
  f.install();
  const std::shared_ptr<SessionT> session = f.holder.borrow();
  CHECK(session != nullptr);
  CHECK(f.startTask());
  const int pending = f.coord.pendingCount();
  CHECK(pending == 2);

  ScriptedSocket* sock = f.sock;
  sock->handler = [sock, pending](const std::string&, const std::string& url,
                                  const std::string&) {
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    if (url.find("today") != std::string::npos)
      return rsp(200, todayBody("task-1", "in_progress", 3));
    if (url.find("events/batch") != std::string::npos) {
      if (sock->events_calls == 1) return rsp(401, "");
      return rsp(200, batchBody(pending, {"ev-1", "ev-2"}, "Accepted"));
    }
    return rsp(404, "");
  };

  f.armProbe();
  CHECK(session->runOnlineCycle(f.lockFn(), f.unlockFn()));

  CHECK(sock->challenge_calls == 2);   // the refresh really happened
  CHECK(sock->events_calls == 2);      // initial 401 + one retry
  CHECK(f.storage.ops_unlocked == 0);  // the refresh read NO counters
  CHECK(f.storage.ops_locked > 0);
  CHECK(f.sock->net_calls_while_locked == 0);
  return true;
}

// 4) stale session branch: a superseded session must not send, and must not
//    read counters outside the lock either.
static bool run_case_probe_cycle_stale_session_reads_counters_locked() {
  Fixture f;
  CHECK(f.seedTask());
  f.install();
  const std::shared_ptr<SessionT> session = f.holder.borrow();
  CHECK(session != nullptr);
  ScriptedSocket* old_sock = f.sock;
  f.install();  // reconfigure: the first session is superseded
  CHECK(session->superseded());

  f.armProbe();
  CHECK(!session->runOnlineCycle(f.lockFn(), f.unlockFn()));

  CHECK(old_sock->net_calls == 0);     // a dead session never sends
  CHECK(f.storage.ops_unlocked == 0);  // and never reads counters unlocked
  return true;
}

static bool run_case_all() {
  // FINAL-CONCURRENCY-CLEANUP FIX-1
  CASE(diagnostics_publish_is_atomic_with_ownership_check);
  CASE(stale_diagnostics_cannot_publish_after_reconfigure_between_check_and_write);
  // FINAL-CONCURRENCY-CLEANUP FIX-2
  CASE(probe_cycle_initial_auth_and_today_reads_counters_locked);
  CASE(probe_cycle_events_ack_reads_counters_locked);
  CASE(probe_cycle_401_reauth_reads_counters_locked);
  CASE(probe_cycle_stale_session_reads_counters_locked);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 final concurrency cleanup tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
