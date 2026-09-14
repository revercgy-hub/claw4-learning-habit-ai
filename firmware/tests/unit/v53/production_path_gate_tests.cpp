// claw4/firmware/tests/unit/v53/production_path_gate_tests.cpp
// WB-V53-NEXT-001 REVIEW-FIX-002 — CP5 final gate over the DEVICE production
// orchestration path.
//
// The RF6 gate (backend_session_gate_tests.cpp) drives the legacy
// single-threaded pullToday() / syncOnce() entry points. The DEVICE uses the
// lock-injected path instead, and that is exactly where the R7 races lived:
//
//   LearningBackendSession::runOnlineCycle(lock, unlock)
//     -> pullTodayLocked()          ownership check + applyTodaySnapshot in ONE
//                                   state-lock transaction (R7.1)
//     -> syncOnceLocked()
//     -> SyncExecutor::runCycle()   prepare LOCKED / send UNLOCKED / apply LOCKED,
//                                   bound to a FROZEN expected generation (R7.2),
//                                   re-gated after the credential refresh (R7.3)
//     -> BackendClient              real endpoint paths, JSON encode/decode,
//                                   status classification, credential flow
//     -> sync::HttpTransport        *** the ONLY stubbed layer (socket) ***
//     -> AppCoordinator
//
// Reconfiguration is performed exactly the way the device does it
// (LearningRuntime::ConfigureBackend): state lock held -> beginNewSession() ->
// install a fresh lease + session. That ordering is what makes the lock-order
// claim testable rather than aspirational.
//
// Deterministic: blocking uses condition_variable latches, never sleeps.
//
// Why not a real TCP loopback: this host refuses to EXECUTE any binary that
// creates sockets, so loopback stays ENV_VERIFY_REQUIRED (see the report). The
// stub replaces ONLY the socket; the layer boundary is stated, not hidden.

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
const char* kUrl = "http://127.0.0.1:8000";  // loopback form; socket is stubbed
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

// Real wire shape (wire_codec.cpp DecodeBatchResponse): event_id / sequence /
// lowercase status token / http_status, plus last_acked_sequence + server_time.
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

// A coordinator-level transport used to drive SyncExecutor directly.
class CountingSyncTransport final : public SyncTransport {
 public:
  int sends = 0;

  SyncClient::Response send(const SyncClient::Request& request) override {
    ++sends;
    SyncClient::Response r;
    if (sends == 1) {
      r.error_class = SyncErrorClass::Auth;
      r.http_status = 401;
      return r;
    }
    r.error_class = SyncErrorClass::None;
    r.http_status = 200;
    int64_t ack = request.last_acked_sequence;
    for (const auto& e : request.events) {
      PerEventResult per;
      per.event_id = e.event_id;
      per.sequence = e.sequence;
      per.outcome = EventOutcome::Accepted;
      per.http_status = 200;
      r.batch.results.push_back(per);
      ack = e.sequence;
    }
    r.batch.last_acked_sequence = ack;
    return r;
  }
};

// ---------------------------------------------------------------------------
// the one stubbed layer: the socket
// ---------------------------------------------------------------------------
class ScriptedSocket final : public HttpTransport {
 public:
  std::function<HttpResponse(const std::string& method, const std::string& url,
                             const std::string& body)>
      handler;
  std::vector<std::string> calls;
  int challenge_calls = 0;
  int auth_calls = 0;
  int today_calls = 0;
  int events_calls = 0;

  HttpResponse request(const std::string& method, const std::string& url,
                       const std::vector<std::pair<std::string, std::string>>&,
                       const std::string& body, int64_t) override {
    calls.push_back(method + " " + url);
    if (url.find("challenge") != std::string::npos) ++challenge_calls;
    if (url.find("/devices/auth") != std::string::npos) ++auth_calls;
    if (url.find("today") != std::string::npos) ++today_calls;
    if (url.find("events/batch") != std::string::npos) ++events_calls;
    if (!handler) return HttpResponse{};
    return handler(method, url, body);
  }

  int count(const std::string& needle) const {
    int n = 0;
    for (const auto& c : calls) {
      if (c.find(needle) != std::string::npos) ++n;
    }
    return n;
  }
};

// ---------------------------------------------------------------------------
// a latch a worker (or the storage layer) can block on, released by the test
// ---------------------------------------------------------------------------
struct Gate {
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
  ~Gate() { release(); }
};

// Releases the gate before joining, so a failing CHECK can never terminate the
// process on a joinable thread.
struct WorkerJoiner {
  std::thread worker;
  Gate* gate = nullptr;
  ~WorkerJoiner() {
    if (gate != nullptr) gate->release();
    if (worker.joinable()) worker.join();
  }
};

// An instrumented state lock: a test can ask whether the injected lock is
// currently HELD. Only the session thread mutates the depth, so reading it from
// the same thread is exact (and avoids the undefined behaviour of try_lock on a
// mutex the calling thread already owns).
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

// Reports the lease state and, on the first probe after `armed` is set, records
// whether the injected state lock was held at that moment.
class ProbingLeaseSource final : public SessionLeaseSource {
 public:
  LockDepth* lock_depth = nullptr;
  SessionLease lease{};
  mutable bool armed = false;
  mutable bool probed = false;
  mutable bool probed_while_locked = false;

  SessionLease currentLease() const override { return lease; }

  bool isCurrent(const SessionLease& candidate) const override {
    if (armed && !probed) {
      probed = true;
      probed_while_locked = lock_depth != nullptr && lock_depth->held();
    }
    return candidate.matches(lease);
  }
};

// A storage wrapper that lets a test observe/pause INSIDE a commit, i.e. while
// the caller's state critical section is held.
class GatedCommitStorage final : public OutboxStorage {
 public:
  explicit GatedCommitStorage(std::shared_ptr<FakeDisk> disk)
      : inner_(std::move(disk)) {}

  std::function<void()> on_commit;  // runs with the state lock HELD

  bool load(OutboxState& out) override { return inner_.load(out); }

  CommitStatus commit(const DomainState& next_domain,
                      const std::vector<PendingEvent>& appended,
                      int64_t next_sequence) override {
    if (on_commit) on_commit();
    return inner_.commit(next_domain, appended, next_sequence);
  }
  CommitStatus commitDiagnostic(bool sync_failed, bool sync_recovered) override {
    return inner_.commitDiagnostic(sync_failed, sync_recovered);
  }
  CommitStatus removeAcked(int64_t up_to_sequence) override {
    return inner_.removeAcked(up_to_sequence);
  }
  CommitStatus markDeadLetter(const claw4::domain::EventId& event_id,
                              const std::string& reason) override {
    return inner_.markDeadLetter(event_id, reason);
  }

 private:
  FakeOutboxStorage inner_;
};

// ---------------------------------------------------------------------------
// fixture: production wiring with the device's lock-injection discipline
// ---------------------------------------------------------------------------
template <typename StorageT>
struct Fixture {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  StorageT storage{disk};
  DomainReducer reducer;
  AppCoordinator coord{storage, reducer};
  SessionLeaseHolder<SessionT> holder;
  std::mutex state_mutex;
  ScriptedSocket* sock = nullptr;
  int64_t e = 0;

  StateLockFn lockFn() {
    return [this] { state_mutex.lock(); };
  }
  StateUnlockFn unlockFn() {
    return [this] { state_mutex.unlock(); };
  }

  // Mirrors LearningRuntime::ConfigureBackend(): the generation bump and the
  // lease install both happen UNDER the state lock.
  SessionLease install() {
    std::lock_guard<std::mutex> guard(state_mutex);
    return installLocked();
  }

  SessionLease installLocked() {
    const int64_t generation = coord.beginNewSession();
    return holder.installWith(generation, [this](const SessionLease& lease) {
      auto transport = std::make_unique<ScriptedSocket>();
      sock = transport.get();
      return std::make_shared<SessionT>(
          coord, std::move(transport), kUrl, DeviceId{"dev-1"},
          ChildId{"child-1"}, &sign,
          [] { return int64_t{1700000001}; }, lease, &holder);
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
  bool pauseTask() {
    IntentRequest r;
    r.intent = Intent::Pause;
    r.task_id = TaskId{"task-1"};
    return coord.dispatchIntent(r, ctx()).ok;
  }
};

using Path = Fixture<FakeOutboxStorage>;
using AtomicPath = Fixture<GatedCommitStorage>;

// A default happy-path handler (challenge/auth/today/events all succeed).
void happyHandler(ScriptedSocket* sock, const int64_t ack_upto,
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

}  // namespace

// ---------------------------------------------------------------------------
// Gate A: a normal online cycle over the production lock-injected path
// ---------------------------------------------------------------------------
static bool run_case_gate_a_production_online_cycle() {
  Path p;
  CHECK(p.seedTask());
  p.install();
  const std::shared_ptr<SessionT> session = p.holder.borrow();
  CHECK(session != nullptr);
  CHECK(p.startTask());
  CHECK(p.coord.pendingCount() == 2);
  happyHandler(p.sock, 2, {"ev-1", "ev-2"});

  CHECK(session->runOnlineCycle(p.lockFn(), p.unlockFn()));
  CHECK(p.coord.state().tasks.size() == 1);
  CHECK(p.coord.pendingCount() == 0);
  CHECK(p.coord.lastAcked() == 2);
  CHECK(p.sock->today_calls == 1);
  CHECK(p.sock->events_calls == 1);
  // The session froze the generation it was installed with.
  CHECK(session->sessionGeneration() == p.coord.generation());
  CHECK(!session->superseded());
  return true;
}

// ---------------------------------------------------------------------------
// Gate B: events 401 -> reauth -> reconfigure DURING the refresh -> the dead
// session must not issue a second POST and must not touch the new session.
// ---------------------------------------------------------------------------
static bool run_case_gate_b_reconfigure_during_reauth_cannot_retry() {
  Path p;
  CHECK(p.seedTask());
  p.install();
  const std::shared_ptr<SessionT> session = p.holder.borrow();
  CHECK(session != nullptr);
  CHECK(p.startTask());
  const int pending = p.coord.pendingCount();
  CHECK(pending == 2);

  Gate gate;
  const int64_t generation_before = p.coord.generation();
  ScriptedSocket* old_sock = p.sock;
  old_sock->handler = [&](const std::string&, const std::string& url,
                          const std::string&) {
    if (url.find("challenge") != std::string::npos) {
      // The refresh is the SECOND challenge (the first one belongs to the
      // initial /today authentication): block there.
      if (old_sock->challenge_calls == 2) gate.arriveAndWait();
      return rsp(200, kChallenge);
    }
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    if (url.find("events/batch") != std::string::npos) return rsp(401, "");
    return rsp(200, todayBody("task-1", "in_progress", 3));
  };

  bool ok = false;
  WorkerJoiner joiner;
  joiner.gate = &gate;
  joiner.worker = std::thread(
      [&] { ok = session->runOnlineCycle(p.lockFn(), p.unlockFn()); });

  // The worker is now inside the credential refresh's network wait, holding no
  // lock. Reconfigure exactly like ConfigureBackend() does.
  gate.waitUntilEntered();
  p.install();
  gate.release();
  joiner.worker.join();

  CHECK(p.coord.generation() > generation_before);  // the reconfigure happened
  CHECK(!ok);
  CHECK(old_sock->events_calls == 1);   // never a second POST
  CHECK(p.sock->events_calls == 0);     // the new session sent nothing
  CHECK(p.coord.lastAcked() == 0);      // no ACK from the dead session
  CHECK(p.coord.pendingCount() == pending);
  CHECK(!p.coord.authPaused());         // the new session was NOT paused
  CHECK(session->superseded());
  return true;
}

// ---------------------------------------------------------------------------
// Gate C1: reconfigure after the /today response exists -> the stale snapshot
// must be rejected and nothing written.
// ---------------------------------------------------------------------------
static bool run_case_gate_c_reconfigure_before_today_apply_rejects_stale() {
  Path p;
  CHECK(p.seedTask());
  p.install();
  const std::shared_ptr<SessionT> session = p.holder.borrow();
  CHECK(session != nullptr);
  CHECK(p.startTask());
  const int pending = p.coord.pendingCount();

  Gate gate;
  p.sock->handler = [&](const std::string&, const std::string& url,
                        const std::string&) {
    if (url.find("today") != std::string::npos) {
      gate.arriveAndWait();  // reconfigure lands before the response is used
      return rsp(200, todayBody("task-9", "ready", 1));
    }
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    return rsp(200, batchBody(0, {}, "Accepted"));
  };

  bool ok = true;
  WorkerJoiner joiner;
  joiner.gate = &gate;
  joiner.worker = std::thread(
      [&] { ok = session->runOnlineCycle(p.lockFn(), p.unlockFn()); });

  gate.waitUntilEntered();
  p.install();
  gate.release();
  joiner.worker.join();

  CHECK(!ok);
  CHECK(session->superseded());
  CHECK(session->diagnostics().last_operation == "today_superseded");
  // The stale snapshot ("task-9") never entered the cache.
  for (const auto& t : p.coord.state().tasks) CHECK(t.task_id != TaskId{"task-9"});
  CHECK(p.coord.pendingCount() == pending);
  CHECK(p.coord.lastAcked() == 0);
  return true;
}

// ---------------------------------------------------------------------------
// Gate C2 (R7.1 atomicity): the ownership check and applyTodaySnapshot are one
// state transaction. Proven directly: pause INSIDE the snapshot commit and
// show that the state lock is held, so ConfigureBackend() — which must take
// that same lock before it can bump the generation — cannot interleave.
// ---------------------------------------------------------------------------
static bool run_case_gate_c2_today_check_and_apply_are_one_critical_section() {
  AtomicPath p;
  CHECK(p.seedTask());
  p.install();
  const std::shared_ptr<SessionT> session = p.holder.borrow();
  CHECK(session != nullptr);
  CHECK(p.startTask());
  happyHandler(p.sock, 2, {"ev-1", "ev-2"});

  Gate in_commit;
  p.storage.on_commit = [&in_commit] { in_commit.arriveAndWait(); };

  bool ok = false;
  WorkerJoiner joiner;
  joiner.gate = &in_commit;
  joiner.worker = std::thread(
      [&] { ok = session->runOnlineCycle(p.lockFn(), p.unlockFn()); });

  // Inside the today-snapshot commit, i.e. between the ownership check and the
  // publish of the new state.
  in_commit.waitUntilEntered();
  {
    // The state lock MUST be held for the whole transaction: this is what makes
    // a concurrent ConfigureBackend() impossible.
    std::unique_lock<std::mutex> probe(p.state_mutex, std::try_to_lock);
    CHECK(!probe.owns_lock());
  }
  // And the session that owns the transaction is still the current one.
  CHECK(p.holder.isInstalled(session.get()));
  in_commit.release();
  joiner.worker.join();

  CHECK(ok);
  CHECK(p.coord.state().tasks.size() == 1);
  CHECK(p.coord.lastAcked() == 2);
  return true;
}

// ---------------------------------------------------------------------------
// Gate D: while the events POST is blocked, a LOCAL command commits; the old
// ACK must only remove the sent prefix and keep the new pending event.
// ---------------------------------------------------------------------------
static bool run_case_gate_d_ack_covers_only_the_sent_prefix() {
  Path p;
  CHECK(p.seedTask());
  p.install();
  const std::shared_ptr<SessionT> session = p.holder.borrow();
  CHECK(session != nullptr);
  CHECK(p.startTask());
  CHECK(p.coord.pendingCount() == 2);

  Gate gate;
  p.sock->handler = [&](const std::string&, const std::string& url,
                        const std::string&) {
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    if (url.find("today") != std::string::npos)
      return rsp(200, todayBody("task-1", "in_progress", 3));
    if (url.find("events/batch") != std::string::npos) {
      gate.arriveAndWait();  // the request is on the wire with NO lock held
      return rsp(200, batchBody(2, {"ev-1", "ev-2"}, "Accepted"));
    }
    return rsp(404, "");
  };

  bool ok = false;
  WorkerJoiner joiner;
  joiner.gate = &gate;
  joiner.worker = std::thread(
      [&] { ok = session->runOnlineCycle(p.lockFn(), p.unlockFn()); });

  gate.waitUntilEntered();
  {
    // The UI/state owner keeps making progress: a local Pause commits while the
    // network request is suspended (this is the whole point of A03).
    std::lock_guard<std::mutex> guard(p.state_mutex);
    CHECK(p.pauseTask());
  }
  CHECK(p.coord.pendingCount() == 3);
  gate.release();
  joiner.worker.join();

  CHECK(ok);
  CHECK(p.sock->events_calls == 1);
  // Only the two events that were actually SENT are ACKed.
  CHECK(p.coord.lastAcked() == 2);
  CHECK(p.coord.pendingCount() == 1);
  CHECK(p.disk->state.pending.front().sequence == 3);
  return true;
}

// ---------------------------------------------------------------------------
// Gate C3 (R7.1, discriminating): prove the /today ownership check runs INSIDE
// the apply transaction, not before it.
//
// Against the pre-fix code the order was
//     superseded()  [NO lock]  ->  lock()  ->  applyTodaySnapshot()
// so the lease question was answered with the state lock FREE, which is exactly
// the window a concurrent ConfigureBackend() could slip through. The lease
// source below records whether the injected lock was held when the session
// asked "am I still current?", so this case FAILS on the old ordering and
// passes only when the check moved into the critical section.
// ---------------------------------------------------------------------------
static bool run_case_gate_c3_today_ownership_check_runs_inside_transaction() {
  auto disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  AppCoordinator coord{storage, reducer};
  LockDepth lock_depth;

  ProbingLeaseSource lease_source;
  lease_source.lock_depth = &lock_depth;
  lease_source.lease = SessionLease{1, coord.beginNewSession()};

  ScriptedSocket* sock = nullptr;
  auto transport = std::make_unique<ScriptedSocket>();
  sock = transport.get();
  sock->handler = [&](const std::string&, const std::string& url,
                      const std::string&) {
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    if (url.find("today") != std::string::npos) {
      // Arm the probe: the NEXT lease question must be asked from inside the
      // apply critical section.
      lease_source.armed = true;
      return rsp(200, todayBody("task-1", "ready", 3));
    }
    return rsp(200, batchBody(0, {}, "Accepted"));
  };

  LearningBackendSession session(
      coord, std::move(transport), kUrl, DeviceId{"dev-1"},
      ChildId{"child-1"}, &sign, [] { return int64_t{1700000001}; },
      lease_source.lease, &lease_source);

  CHECK(session.runOnlineCycle([&] { lock_depth.lock(); },
                              [&] { lock_depth.unlock(); }));
  CHECK(sock->today_calls == 1);
  CHECK(lease_source.probed);
  CHECK(lease_source.probed_while_locked);  // the check is inside the transaction
  CHECK(coord.state().tasks.size() == 1);
  return true;
}

// ---------------------------------------------------------------------------
// Gate B2: the reconfigure lands AFTER the refresh token came back and before
// the retry would be prepared. The session must refuse to report a successful
// refresh, so no retry is prepared at all.
// ---------------------------------------------------------------------------
static bool run_case_gate_b2_reconfigure_after_refresh_token_before_retry_is_stale() {
  Path p;
  CHECK(p.seedTask());
  p.install();
  const std::shared_ptr<SessionT> session = p.holder.borrow();
  CHECK(session != nullptr);
  CHECK(p.startTask());
  const int pending = p.coord.pendingCount();

  Gate gate;
  ScriptedSocket* old_sock = p.sock;
  old_sock->handler = [&](const std::string&, const std::string& url,
                          const std::string&) {
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) {
      // The REFRESH auth exchange (the first one belongs to /today): pause with
      // the refreshed token already issued, before the retry is prepared.
      if (old_sock->auth_calls == 2) gate.arriveAndWait();
      return rsp(200, kAuth);
    }
    if (url.find("events/batch") != std::string::npos) return rsp(401, "");
    return rsp(200, todayBody("task-1", "in_progress", 3));
  };

  bool ok = true;
  WorkerJoiner joiner;
  joiner.gate = &gate;
  joiner.worker = std::thread(
      [&] { ok = session->runOnlineCycle(p.lockFn(), p.unlockFn()); });

  gate.waitUntilEntered();
  p.install();
  gate.release();
  joiner.worker.join();

  CHECK(!ok);
  CHECK(old_sock->events_calls == 1);  // the retry was never prepared
  CHECK(p.sock->events_calls == 0);
  CHECK(p.coord.lastAcked() == 0);
  CHECK(p.coord.pendingCount() == pending);
  CHECK(!p.coord.authPaused());
  return true;
}

// ---------------------------------------------------------------------------
// Gate B3 (R7.2, discriminating): drive SyncExecutor directly so the hook can
// be placed EXACTLY between "the refresh reported success" and "prepare the
// retry". Pre-fix, the second attempt called prepareSync() and inherited the
// NEW generation, sending a request on behalf of a dead session.
// ---------------------------------------------------------------------------
static bool run_case_gate_b3_retry_never_inherits_new_generation() {
  Path p;
  CHECK(p.seedTask());
  CHECK(p.startTask());
  const int pending = p.coord.pendingCount();
  CHECK(pending == 2);

  CountingSyncTransport transport;
  const int64_t generation_before = p.coord.generation();
  SyncExecutor executor(p.coord, transport, p.lockFn(), p.unlockFn(),
                        p.coord.generation());

  const SyncCycleResult cycle = executor.runCycle([&] {
    // The refresh "succeeds" — but the runtime is reconfigured inside it, so
    // the session that began this cycle is no longer the owner.
    p.install();
    return true;
  });

  CHECK(p.coord.generation() > generation_before);
  CHECK(cycle.outcome == SyncOutcome::StaleResult);
  CHECK(cycle.transport_calls == 1);  // never a second POST
  CHECK(transport.sends == 1);
  CHECK(p.coord.lastAcked() == 0);    // nothing written into the new session
  CHECK(p.coord.pendingCount() == pending);
  return true;
}

static bool run_case_all() {
  CASE(gate_a_production_online_cycle);
  CASE(gate_b_reconfigure_during_reauth_cannot_retry);
  CASE(gate_b2_reconfigure_after_refresh_token_before_retry_is_stale);
  CASE(gate_b3_retry_never_inherits_new_generation);
  CASE(gate_c_reconfigure_before_today_apply_rejects_stale);
  CASE(gate_c2_today_check_and_apply_are_one_critical_section);
  CASE(gate_c3_today_ownership_check_runs_inside_transaction);
  CASE(gate_d_ack_covers_only_the_sent_prefix);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 RF2 production-path orchestration gate ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
