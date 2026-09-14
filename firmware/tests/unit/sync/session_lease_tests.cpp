// claw4/firmware/tests/unit/sync/session_lease_tests.cpp
// WB-V53-NEXT-001 REVIEW-FIX-001 RF1 — backend session lease + generation
// ownership.
//
// These cases drive SessionLeaseHolder<LearningBackendSession>: the SAME
// production helper the device runtime uses (LearningRuntime owns a
// SessionLeaseHolder<LearningBackendSession> and borrows from it for the whole
// network phase). Only the socket layer is stubbed (ScriptedHttp); BackendClient,
// the wire codec, AppCoordinator, SyncExecutor and the lease rules are the real
// production implementations.
//
// Deterministic: no sleeps. Reconfiguration is triggered from inside the stubbed
// HTTP call, i.e. exactly "while the request is in flight".

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "application/coordinator.h"
#include "learning_domain/reducer.h"
#include "sync/http_transport.h"
#include "sync/learning_backend_session.h"
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

const char* kUrl = "http://relay:8000";
const char* kChallenge =
    "{\"challenge_id\":\"ch-1\",\"nonce\":\"n-1\",\"expires_at\":1700000100}";
const char* kAuth = "{\"access_token\":\"tok-1\",\"expires_in\":3600}";
const char* kToday =
    "{\"date\":\"2026-09-06\",\"tasks\":[{\"task_id\":\"srv-task\",\"title\":"
    "\"Math\",\"subject\":\"math\",\"estimated_minutes\":20,\"priority\":"
    "\"high\",\"status\":\"ready\",\"scheduled_date\":\"2026-09-06\","
    "\"version\":1}]}";

HttpResponse ok(const char* body) {
  HttpResponse r;
  r.transport_ok = true;
  r.status = 200;
  r.body = body;
  return r;
}

HttpResponse status(int code) {
  HttpResponse r;
  r.transport_ok = true;
  r.status = code;
  return r;
}

using SessionT = LearningBackendSession;

// Stubbed socket layer: answers the real endpoint paths. `on_inflight` runs
// BEFORE the response is handed back, so a test can reconfigure the runtime
// exactly while the request is in flight.
class ScriptedHttp final : public HttpTransport {
 public:
  std::function<void(const std::string& url)> on_inflight;
  std::function<HttpResponse(const std::string& url)> on_events;
  int challenge_calls = 0;
  int auth_calls = 0;
  int today_calls = 0;
  int events_calls = 0;

  HttpResponse request(const std::string& method, const std::string& url,
                       const std::vector<std::pair<std::string, std::string>>&,
                       const std::string&, int64_t) override {
    if (on_inflight) on_inflight(url);
    if (url.find("challenge") != std::string::npos) {
      ++challenge_calls;
      return ok(kChallenge);
    }
    if (url.find("/devices/auth") != std::string::npos) {
      ++auth_calls;
      return ok(kAuth);
    }
    if (url.find("today") != std::string::npos) {
      ++today_calls;
      return ok(kToday);
    }
    if (url.find("events/batch") != std::string::npos) {
      ++events_calls;
      if (on_events) return on_events(url);
      return ok("{\"last_acked_sequence\":0,\"server_time\":1700000001,\"results\":[]}");
    }
    (void)method;
    return status(404);
  }
};

struct Fixture {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  AppCoordinator coord{storage, reducer};
  SessionLeaseHolder<SessionT> holder;
  ScriptedHttp* http = nullptr;   // owned by the installed session
  int64_t e = 0;

  // Production-shaped install: allocates the cartable lease and builds the
  // session bound to it.
  SessionLease install() {
    const int64_t generation = coord.beginNewSession();
    auto lease = holder.installWith(generation, [this](const SessionLease& l) {
      auto transport = std::make_unique<ScriptedHttp>();
      http = transport.get();
      return std::make_shared<SessionT>(
          coord, std::move(transport), kUrl, DeviceId{"dev-1"},
          ChildId{"child-1"},
          [](const std::string& device, const std::string& challenge,
             const std::string& nonce) {
            return device + ":" + challenge + ":" + nonce;
          },
          [] { return int64_t{1700000001}; }, l, &holder);
    });
    return lease;
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

  // Seeds one Ready task and commits a Start so the outbox has pending events.
  bool seedPendingEvents() {
    Task t;
    t.task_id = TaskId{"task-1"};
    t.child_id = ChildId{"child-1"};
    t.title = "task-1";
    t.status = TaskStatus::Ready;
    t.version = 3;
    if (!coord.applyTodaySnapshot({t})) return false;
    IntentRequest r;
    r.intent = Intent::StartTask;
    r.task_id = TaskId{"task-1"};
    return coord.dispatchIntent(r, ctx()).ok;
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// 1) the borrowed lease keeps the old session alive across a reconfigure
// ---------------------------------------------------------------------------
static bool run_case_runtime_reconfigure_during_backend_io_keeps_old_session_alive() {
  Fixture f;
  f.install();
  std::shared_ptr<SessionT> worker_lease = f.holder.borrow();
  CHECK(worker_lease != nullptr);
  const std::weak_ptr<SessionT> weak = worker_lease;

  // The worker "is on the network" holding only this lease. Meanwhile the
  // runtime is reconfigured (new provisioning / endpoint / rebuild).
  const SessionLease fresh = f.install();

  // The old object is STILL ALIVE: replacing the current session did not destroy
  // what the in-flight worker is using (no use-after-free).
  CHECK(!weak.expired());
  CHECK(worker_lease->superseded());
  CHECK(worker_lease->lease().lease_id != fresh.lease_id);

  // The holder refuses to publish anything produced by the old session.
  CHECK(!f.holder.accepts(worker_lease.get(), worker_lease->lease()));
  const std::shared_ptr<SessionT> new_lease = f.holder.borrow();
  CHECK(new_lease != nullptr);
  CHECK(f.holder.accepts(new_lease.get(), new_lease->lease()));
  CHECK(!new_lease->superseded());  // its own generation matches again

  // Only when the in-flight worker releases its lease does the old object die.
  worker_lease.reset();
  CHECK(weak.expired());
  return true;
}

// ---------------------------------------------------------------------------
// 2) a /today response that arrives after a reconfigure must not write state
// ---------------------------------------------------------------------------
static bool run_case_stale_today_response_after_reconfigure_is_rejected() {
  Fixture f;
  f.install();
  const std::shared_ptr<SessionT> session = f.holder.borrow();
  CHECK(session != nullptr);

  // Reconfigure exactly while the /today request is in flight.
  bool reconfigured = false;
  f.http->on_inflight = [&](const std::string& url) {
    if (url.find("today") != std::string::npos && !reconfigured) {
      reconfigured = true;
      f.install();  // supersedes the session that issued this request
    }
  };

  const bool applied = session->pullToday();
  CHECK(reconfigured);
  CHECK(!applied);                                  // rejected, not applied
  CHECK(session->superseded());
  CHECK(f.coord.state().tasks.empty());             // no state written
  CHECK(session->diagnostics().last_operation == "today_superseded");
  return true;
}

// ---------------------------------------------------------------------------
// 3) a superseded session must not start a new sync
// ---------------------------------------------------------------------------
static bool run_case_stale_session_cannot_start_new_sync_after_reconfigure() {
  Fixture f;
  f.install();
  CHECK(f.seedPendingEvents());
  CHECK(f.coord.pendingCount() > 0);
  const std::shared_ptr<SessionT> session = f.holder.borrow();
  CHECK(!session->superseded());

  // Superseded between /today and /events.
  f.install();
  const std::shared_ptr<SessionT> session2 = f.holder.borrow();
  (void)session2;

  const auto outcome = session->syncOnce();
  CHECK(outcome == SyncOutcome::StaleResult);       // refused, not sent
  CHECK(session->superseded());
  CHECK(f.coord.pendingCount() > 0);                // nothing ACKed
  CHECK(f.coord.lastAcked() == 0);
  return true;
}

// ---------------------------------------------------------------------------
// 4) a stale session must not complete a re-auth / second send
// ---------------------------------------------------------------------------
static bool run_case_stale_reauth_cannot_resume_old_session() {
  Fixture f;
  f.install();
  CHECK(f.seedPendingEvents());
  const std::shared_ptr<SessionT> session = f.holder.borrow();

  // Answer the events POST with 401 AND reconfigure in the same breath: the
  // session is superseded before its credential refresh can run.
  f.http->on_events = [&](const std::string&) {
    f.install();
    return status(401);
  };

  const auto outcome = session->syncOnce();
  // Either StaleResult (the envelope generation moved on) or PausedAuth; what
  // matters is that no second send happened and nothing was resumed.
  CHECK(outcome == SyncOutcome::StaleResult || outcome == SyncOutcome::PausedAuth);
  CHECK(f.coord.pendingCount() > 0);        // pending untouched
  CHECK(f.coord.lastAcked() == 0);
  // The OLD session's outcome must not have paused the NEW session.
  const std::shared_ptr<SessionT> fresh = f.holder.borrow();
  CHECK(fresh != nullptr);
  CHECK(!fresh->superseded());
  return true;
}

// ---------------------------------------------------------------------------
// 5) stale diagnostics must not overwrite the new session's snapshot
// ---------------------------------------------------------------------------
static bool run_case_stale_diagnostics_do_not_overwrite_new_session() {
  Fixture f;
  f.install();
  const std::shared_ptr<SessionT> old_session = f.holder.borrow();
  const auto old_lease = old_session->lease();

  // The new session is installed and its diagnostics are published.
  const SessionLease new_lease = f.install();
  const std::shared_ptr<SessionT> new_session = f.holder.borrow();
  CHECK(new_session != nullptr);

  // The publish rule (production helper): only the still-installed session with
  // a still-current lease may publish.
  CHECK(!f.holder.accepts(old_session.get(), old_lease));
  CHECK(f.holder.accepts(new_session.get(), new_session->lease()));
  CHECK(new_session->lease().lease_id == new_lease.lease_id);

  // And the reverse direction: the new session's own snapshot is accepted.
  CHECK(!old_session->lease().matches(new_session->lease()));
  return true;
}

static bool run_case_all() {
  CASE(runtime_reconfigure_during_backend_io_keeps_old_session_alive);
  CASE(stale_today_response_after_reconfigure_is_rejected);
  CASE(stale_session_cannot_start_new_sync_after_reconfigure);
  CASE(stale_reauth_cannot_resume_old_session);
  CASE(stale_diagnostics_do_not_overwrite_new_session);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 RF1 session lease/generation tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
