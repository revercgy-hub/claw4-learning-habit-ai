// claw4/firmware/tests/unit/sync/sync_executor_tests.cpp
// WB-V53-NEXT-001 CP1 (A03) — host tests for the production sync cycle.
//
// Deterministic, latch-based (no sleeps): the fake transport blocks inside the
// network phase until the test releases it, so the assertions about "the state
// lock is free while the network is suspended" are exact, not timing guesses.
//
// These cases drive SyncExecutor — the same code the device backend worker
// uses — so they cover production behaviour rather than a stand-in.

#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "application/coordinator.h"
#include "learning_domain/reducer.h"
#include "fakes/fake_outbox_storage.h"
#include "sync/sync_executor.h"

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

// ---------------------------------------------------------------------------
// environment
// ---------------------------------------------------------------------------
struct CtxGen {
  DeviceId device{"dev-1"};
  ChildId child{"child-1"};
  int64_t epoch = 1756718530;
  int64_t mono = 1'000'000;
  int e = 0;
  int s = 0;

  ReducerContext make() {
    ReducerContext c;
    c.device_id = device;
    c.child_id = child;
    c.now_epoch = epoch;
    c.monotonic_ms = mono;
    c.make_event_id = [this]() { return EventId{"ev-" + std::to_string(++e)}; };
    c.make_session_id = [this]() { return SessionId{"sess-" + std::to_string(++s)}; };
    return c;
  }
  void advance(int64_t ms) { mono += ms; }
};

Task readyTask(const std::string& id, int version) {
  Task t;
  t.task_id = TaskId{id};
  t.child_id = ChildId{"child-1"};
  t.title = "task-" + id;
  t.status = TaskStatus::Ready;
  t.version = version;
  return t;
}

IntentRequest startOf(const std::string& tid = "task-1") {
  IntentRequest r;
  r.intent = Intent::StartTask;
  r.task_id = TaskId{tid};
  return r;
}

IntentRequest pauseOf(const std::string& tid = "task-1") {
  IntentRequest r;
  r.intent = Intent::Pause;
  r.task_id = TaskId{tid};
  return r;
}

struct Env {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  CoordinatorOptions opts;
  CtxGen ctx;

  AppCoordinator make() { return AppCoordinator{storage, reducer, opts}; }
};

bool seedTask(Env& env, const Task& t) {
  auto c = env.make();
  return c.applyTodaySnapshot({t});
}

// ---------------------------------------------------------------------------
// latch gate + blocking transport
// ---------------------------------------------------------------------------
struct Gate {
  std::mutex mutex;
  std::condition_variable cv;
  bool entered = false;   // transport is inside the network phase
  bool release = false;   // test allows it to finish
};

// Releases the gate and joins on every exit path, so a failing CHECK can never
// terminate the process on a joinable thread.
struct WorkerJoiner {
  std::thread worker;
  Gate* gate = nullptr;
  ~WorkerJoiner() {
    if (worker.joinable()) {
      {
        std::lock_guard<std::mutex> lock(gate->mutex);
        gate->release = true;
      }
      gate->cv.notify_all();
      worker.join();
    }
  }
};

class BlockingSyncTransport final : public SyncTransport {
 public:
  explicit BlockingSyncTransport(Gate& gate) : gate_(gate) {}

  SyncClient::Response send(const SyncClient::Request& request) override {
    ++sends;
    last_request = request;
    {
      std::unique_lock<std::mutex> lock(gate_.mutex);
      gate_.entered = true;
      gate_.cv.notify_all();
      gate_.cv.wait(lock, [this] { return gate_.release; });
    }
    // Accept the whole sent prefix (default fake semantics).
    SyncClient::Response r;
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

  int sends = 0;
  SyncClient::Request last_request;
  Gate& gate_;
};

class ProgrammableSyncTransport final : public SyncTransport {
 public:
  std::function<SyncClient::Response(const SyncClient::Request&)> on_send;
  int sends = 0;

  SyncClient::Response send(const SyncClient::Request& request) override {
    ++sends;
    return on_send(request);
  }
};

SyncClient::Response acceptAll(const SyncClient::Request& request) {
  SyncClient::Response r;
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

// ---------------------------------------------------------------------------
// cases
// ---------------------------------------------------------------------------

// The core A03 acceptance: while the transport is suspended inside the network
// phase, the state lock is FREE, so local commands and UI snapshots keep
// progressing. Deterministic (latch, not sleep).
static bool run_case_cycle_holds_no_state_lock_during_network() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto coord = env.make();
  CHECK(coord.dispatchIntent(startOf(), env.ctx.make()).ok);  // seq 1,2
  CHECK(coord.pendingCount() == 2);

  Gate gate;
  BlockingSyncTransport transport(gate);
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); }, coord.generation());

  SyncCycleResult cycle;
  WorkerJoiner joiner;
  joiner.gate = &gate;
  joiner.worker = std::thread([&] { cycle = executor.runCycle([] { return false; }); });

  // Wait until the worker is genuinely inside the network phase.
  {
    std::unique_lock<std::mutex> lock(gate.mutex);
    gate.cv.wait(lock, [&] { return gate.entered; });
  }

  // (a) the state lock must be available to the UI right now.
  {
    std::unique_lock<std::mutex> probe(state_mutex, std::try_to_lock);
    CHECK(probe.owns_lock());
    probe.unlock();
  }
  // (b) a local command must be able to commit while the network is suspended.
  {
    std::lock_guard<std::mutex> guard(state_mutex);
    CHECK(coord.dispatchIntent(pauseOf(), env.ctx.make()).ok);
  }
  CHECK(coord.pendingCount() == 3);
  CHECK(transport.sends == 1);  // still exactly one request in flight
  CHECK(transport.last_request.events.size() == 2);

  // Release the network phase and let the cycle finish.
  {
    std::lock_guard<std::mutex> lock(gate.mutex);
    gate.release = true;
  }
  gate.cv.notify_all();
  joiner.worker.join();

  CHECK(cycle.transport_called);
  CHECK(cycle.transport_calls == 1);
  CHECK(cycle.outcome == SyncOutcome::Synced);
  CHECK(!cycle.stale);
  CHECK(cycle.generation == 0);
  // Only the two events actually sent were ACKed; the one queued while the
  // network was suspended is retained.
  CHECK(coord.lastAcked() == 2);
  CHECK(coord.pendingCount() == 1);
  return true;
}

static bool run_case_cycle_applies_ack_and_clears_pending() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto coord = env.make();
  CHECK(coord.dispatchIntent(startOf(), env.ctx.make()).ok);

  ProgrammableSyncTransport transport;
  transport.on_send = acceptAll;
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); }, coord.generation());

  const SyncCycleResult cycle = executor.runCycle([] { return false; });
  CHECK(cycle.outcome == SyncOutcome::Synced);
  CHECK(cycle.transport_calls == 1);
  CHECK(coord.pendingCount() == 0);
  CHECK(coord.lastAcked() == 2);
  CHECK(env.disk->state.diagnostic.sync_recovered);
  return true;
}

static bool run_case_cycle_rejects_stale_generation_without_writing() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto coord = env.make();
  CHECK(coord.dispatchIntent(startOf(), env.ctx.make()).ok);

  Gate gate;
  BlockingSyncTransport transport(gate);
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); }, coord.generation());

  SyncCycleResult cycle;
  WorkerJoiner joiner;
  joiner.gate = &gate;
  joiner.worker = std::thread([&] { cycle = executor.runCycle([] { return false; }); });

  {
    std::unique_lock<std::mutex> lock(gate.mutex);
    gate.cv.wait(lock, [&] { return gate.entered; });
  }
  // Runtime reconfigured while the request was in flight.
  coord.beginNewSession();

  {
    std::lock_guard<std::mutex> lock(gate.mutex);
    gate.release = true;
  }
  gate.cv.notify_all();
  joiner.worker.join();

  CHECK(cycle.outcome == SyncOutcome::StaleResult);
  CHECK(cycle.stale);
  CHECK(coord.lastAcked() == 0);       // nothing written into the new session
  CHECK(coord.pendingCount() == 2);
  CHECK(env.disk->ack_calls == 0);
  return true;
}

static bool run_case_cycle_auth_pause_never_calls_transport() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto coord = env.make();
  CHECK(coord.dispatchIntent(startOf(), env.ctx.make()).ok);

  ProgrammableSyncTransport transport;
  transport.on_send = [](const SyncClient::Request&) {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::Auth;
    r.http_status = 401;
    return r;
  };
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); }, coord.generation());

  int reauth_calls = 0;
  const SyncCycleResult first =
      executor.runCycle([&] { ++reauth_calls; return false; });
  CHECK(first.outcome == SyncOutcome::PausedAuth);
  CHECK(reauth_calls == 1);
  CHECK(coord.authPaused());

  // While paused: no send, no re-auth, bounded to zero extra transport calls.
  for (int i = 0; i < 3; ++i) {
    const SyncCycleResult again =
        executor.runCycle([&] { ++reauth_calls; return false; });
    CHECK(again.outcome == SyncOutcome::PausedAuth);
    CHECK(!again.transport_called);
  }
  CHECK(transport.sends == 1);
  CHECK(reauth_calls == 1);
  CHECK(coord.pendingCount() == 2);
  return true;
}

static bool run_case_cycle_reauth_ok_retries_once_without_lock() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto coord = env.make();
  CHECK(coord.dispatchIntent(startOf(), env.ctx.make()).ok);

  ProgrammableSyncTransport transport;
  auto calls = std::make_shared<int>(0);
  transport.on_send = [calls](const SyncClient::Request& request) {
    SyncClient::Response r;
    // First call fails auth; the retry (after re-auth) succeeds.
    ++*calls;
    if (*calls == 1) {
      r.error_class = SyncErrorClass::Auth;
      r.http_status = 401;
      return r;
    }
    return acceptAll(request);
  };
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); }, coord.generation());

  int reauth_calls = 0;
  const SyncCycleResult cycle =
      executor.runCycle([&] { ++reauth_calls; return true; });
  CHECK(cycle.outcome == SyncOutcome::Synced);
  CHECK(reauth_calls == 1);
  CHECK(cycle.transport_calls == 2);  // original + one retry, never more
  CHECK(coord.pendingCount() == 0);
  CHECK(!coord.authPaused());
  return true;
}

static bool run_case_cycle_reauth_fail_pauses() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto coord = env.make();
  CHECK(coord.dispatchIntent(startOf(), env.ctx.make()).ok);

  ProgrammableSyncTransport transport;
  transport.on_send = [](const SyncClient::Request&) {
    SyncClient::Response r;
    r.error_class = SyncErrorClass::Auth;
    r.http_status = 403;
    return r;
  };
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); }, coord.generation());

  int reauth_calls = 0;
  const SyncCycleResult cycle =
      executor.runCycle([&] { ++reauth_calls; return false; });
  CHECK(cycle.outcome == SyncOutcome::PausedAuth);
  CHECK(reauth_calls == 1);
  CHECK(cycle.transport_calls == 1);  // no second send when the refresh failed
  CHECK(coord.authPaused());
  CHECK(coord.pendingCount() == 2);   // pending untouched
  return true;
}

static bool run_case_cycle_no_pending_never_touches_transport() {
  Env env;
  CHECK(seedTask(env, readyTask("task-1", 3)));
  auto coord = env.make();

  ProgrammableSyncTransport transport;
  transport.on_send = acceptAll;
  std::mutex state_mutex;
  SyncExecutor executor(coord, transport,
                        [&] { state_mutex.lock(); },
                        [&] { state_mutex.unlock(); }, coord.generation());

  const SyncCycleResult cycle = executor.runCycle([] { return false; });
  CHECK(cycle.outcome == SyncOutcome::NoPending);
  CHECK(!cycle.transport_called);
  CHECK(transport.sends == 0);
  return true;
}

static bool run_case_all() {
  CASE(cycle_holds_no_state_lock_during_network);
  CASE(cycle_applies_ack_and_clears_pending);
  CASE(cycle_rejects_stale_generation_without_writing);
  CASE(cycle_auth_pause_never_calls_transport);
  CASE(cycle_reauth_ok_retries_once_without_lock);
  CASE(cycle_reauth_fail_pauses);
  CASE(cycle_no_pending_never_touches_transport);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 CP1 sync executor (A03) tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
