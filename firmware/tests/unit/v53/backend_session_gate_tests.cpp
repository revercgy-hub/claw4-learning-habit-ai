// claw4/firmware/tests/unit/v53/backend_session_gate_tests.cpp
// WB-V53-NEXT-001 REVIEW-FIX-001 RF6 — backend-session orchestration gate.
//
// Covers the REAL production chain:
//   LearningBackendSession -> BackendClient (real URLs, JSON encode/decode,
//   status classification, credential flow) -> sync::HttpTransport ->
//   BackendClient -> AppCoordinator/SyncExecutor/SessionLeaseHolder.
//
// Only the SOCKET is stubbed by this file's handler. Every scenario below is the
// real session/client/coordinator/lease code path.
//
// Why not a TCP loopback server: this host refuses to EXECUTE any binary that
// creates sockets ("Permission denied" at exec, consistently — see the review
// report evidence: a WSAStartup-only binary and a std::thread-only binary both
// run, while a socket/bind/listen/connect binary is blocked). The task book
// sanctions the alternative used here; the layer boundary is documented
// explicitly rather than hidden.
//
// Deterministic: no sleeps, no real family backend, synthetic data only.

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

using SessionT = LearningBackendSession;
const char* kUrl = "http://127.0.0.1:8000";  // loopback form; socket is stubbed

HttpResponse rsp(int status, const std::string& body) {
  HttpResponse r;
  r.transport_ok = true;
  r.status = status;
  r.body = body;
  return r;
}

const char* kChallenge =
    "{\"challenge_id\":\"ch-1\",\"nonce\":\"n-1\",\"expires_at\":1700000100}";
const char* kAuth = "{\"access_token\":\"tok-1\",\"expires_in\":3600}";

std::string todayBody(const char* task_id, const char* status, int version) {
  return std::string("{\"date\":\"2026-09-06\",\"tasks\":[{\"task_id\":\"") +
         task_id + "\",\"title\":\"Math\",\"subject\":\"math\"," +
         "\"estimated_minutes\":20,\"priority\":\"high\",\"status\":\"" + status +
         "\",\"scheduled_date\":\"2026-09-06\",\"version\":" +
         std::to_string(version) + "}]}";
}

// Real wire shape (see wire_codec.cpp DecodeBatchResponse): each row needs
// event_id / status (lowercase token) / sequence / http_status, plus the
// envelope's last_acked_sequence and server_time.
std::string lowerToken(const char* token) {
  std::string s(token);
  for (auto& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

std::string batchBody(int64_t last_acked, const std::vector<std::string>& event_ids,
                      const char* outcome) {
  const std::string token = lowerToken(outcome);
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

// The only mocked layer: the socket. It speaks the same method/url/body/status
// contract the device transport does.
class SocketStub final : public HttpTransport {
 public:
  std::function<HttpResponse(const std::string& method, const std::string& url,
                             const std::string& body)>
      handler;
  std::vector<std::string> calls;
  int events_calls = 0;

  HttpResponse request(const std::string& method, const std::string& url,
                       const std::vector<std::pair<std::string, std::string>>&,
                       const std::string& body, int64_t) override {
    calls.push_back(method + " " + url);
    if (url.find("events/batch") != std::string::npos) ++events_calls;
    if (!handler) return {};
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

struct Scenario {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  DomainReducer reducer;
  AppCoordinator coord{storage, reducer};
  SessionLeaseHolder<SessionT> holder;
  SocketStub* sock = nullptr;
  int64_t e = 0;

  // A default handler: challenge/auth/today/events all succeed.
  void defaultHandler() {
    sock->handler = [](const std::string&, const std::string& url,
                       const std::string&) {
      if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
      if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
      if (url.find("today") != std::string::npos)
        return rsp(200, todayBody("task-1", "ready", 3));
      if (url.find("events/batch") != std::string::npos)
        return rsp(200, batchBody(0, {}, "Accepted"));
      return rsp(404, "");
    };
  }

  SessionLease install() {
    const int64_t generation = coord.beginNewSession();
    return holder.installWith(generation, [this](const SessionLease& lease) {
      auto transport = std::make_unique<SocketStub>();
      sock = transport.get();
      defaultHandler();
      return std::make_shared<SessionT>(
          coord, std::move(transport), kUrl, DeviceId{"dev-1"},
          ChildId{"child-1"},
          [](const std::string& device, const std::string& challenge,
             const std::string& nonce) {
            return device + ":" + challenge + ":" + nonce;
          },
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

  bool startTask() {
    IntentRequest r;
    r.intent = Intent::StartTask;
    r.task_id = TaskId{"task-1"};
    return coord.dispatchIntent(r, ctx()).ok;
  }

  bool completeTask() {
    IntentRequest r;
    r.intent = Intent::Complete;
    r.task_id = TaskId{"task-1"};
    return coord.dispatchIntent(r, ctx()).ok;
  }
};

}  // namespace

// 1) authenticate over the real credential flow
static bool run_case_authenticate() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->authenticate());
  CHECK(session->authenticated());
  CHECK(s.sock->count("challenge") == 1);
  CHECK(s.sock->count("/api/v1/devices/auth") == 1);
  CHECK(session->diagnostics().last_operation == "authenticate");
  return true;
}

// 2) GET today populates the authoritative cache
static bool run_case_get_today() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->pullToday());
  CHECK(s.coord.state().tasks.size() == 1);
  CHECK(s.coord.state().tasks[0].task_id == TaskId{"task-1"});
  CHECK(s.sock->count("/today") == 1);
  CHECK(session->diagnostics().last_operation == "today");
  return true;
}

// 3) an offline terminal merge is not undone by a stale today snapshot
static bool run_case_offline_terminal_merge() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->pullToday());
  CHECK(s.startTask());
  CHECK(s.completeTask());
  CHECK(s.coord.state().tasks[0].status == TaskStatus::Completed);
  CHECK(s.coord.pendingCount() > 0);
  // The server still reports the task as Ready (it has not seen the events).
  CHECK(session->pullToday());
  CHECK(s.coord.state().tasks[0].status == TaskStatus::Completed);  // not revived
  return true;
}

// 4) POST events + ACK through the real wire path
static bool run_case_post_events_ack() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->pullToday());
  CHECK(s.startTask());
  const int pending = s.coord.pendingCount();
  s.sock->handler = [pending](const std::string&, const std::string& url,
                              const std::string&) {
    if (url.find("events/batch") != std::string::npos) {
      return rsp(200, batchBody(pending, {"ev-1", "ev-2"}, "Accepted"));
    }
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    return rsp(200, todayBody("task-1", "in_progress", 3));
  };
  const auto outcome = session->syncOnce();
  CHECK(outcome == SyncOutcome::Synced);
  CHECK(s.coord.pendingCount() == 0);
  CHECK(s.coord.lastAcked() == pending);
  CHECK(s.sock->events_calls == 1);
  return true;
}

// 5) 401 -> exactly one re-auth -> retry succeeds
static bool run_case_401_one_reauth() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->pullToday());
  CHECK(s.startTask());
  const int pending = s.coord.pendingCount();
  int auth_flows = 0;
  s.sock->handler = [&](const std::string&, const std::string& url,
                        const std::string&) {
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) {
      ++auth_flows;
      return rsp(200, kAuth);
    }
    if (url.find("events/batch") != std::string::npos) {
      if (s.sock->events_calls == 1) return rsp(401, "");
      return rsp(200, batchBody(pending, {"ev-1", "ev-2"}, "Accepted"));
    }
    return rsp(200, todayBody("task-1", "in_progress", 3));
  };
  const auto first = session->syncOnce();
  // Contract: the first auth failure reports ReauthOk (the credential refresh
  // happened) and the CALLER re-runs the exchange exactly once.
  CHECK(first == SyncOutcome::ReauthOk);
  CHECK(s.sock->events_calls == 1);
  CHECK(s.coord.pendingCount() == pending);  // nothing deleted on 401
  CHECK(!s.coord.authPaused());

  const auto second = session->syncOnce();
  CHECK(second == SyncOutcome::Synced);
  CHECK(s.sock->events_calls == 2);          // exactly one retry, never more
  CHECK(s.coord.pendingCount() == 0);
  CHECK(s.coord.lastAcked() == pending);
  CHECK(!s.coord.authPaused());
  // Every auth call this stub saw is a REFRESH (the initial /today auth was
  // served before this handler was installed): exactly one, despite the 401.
  CHECK(auth_flows == 1);
  // End to end: one initial challenge + exactly one refresh challenge.
  CHECK(s.sock->count("challenge") == 2);
  return true;
}

// 6) a lost response followed by Duplicate converges idempotently
static bool run_case_duplicate_lost_response() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->pullToday());
  CHECK(s.startTask());
  const int pending = s.coord.pendingCount();

  // (a) the response is LOST at the transport level -> Network -> Backoff.
  s.sock->handler = [](const std::string&, const std::string& url,
                       const std::string&) {
    if (url.find("events/batch") != std::string::npos) return HttpResponse{};
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    return rsp(200, todayBody("task-1", "in_progress", 3));
  };
  CHECK(session->syncOnce() == SyncOutcome::Backoff);
  CHECK(s.coord.pendingCount() == pending);  // nothing lost

  // (b) the retry reports everything as Duplicate (server already persisted).
  s.sock->handler = [pending](const std::string&, const std::string& url,
                              const std::string&) {
    if (url.find("events/batch") != std::string::npos) {
      return rsp(200, batchBody(pending, {"ev-1", "ev-2"}, "Duplicate"));
    }
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    return rsp(200, todayBody("task-1", "in_progress", 3));
  };
  CHECK(session->syncOnce() == SyncOutcome::Synced);
  CHECK(s.coord.pendingCount() == 0);
  CHECK(s.coord.lastAcked() == pending);
  return true;
}

// 7) a superseded session cannot send or write
static bool run_case_stale_generation() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->pullToday());
  CHECK(s.startTask());
  const int pending = s.coord.pendingCount();
  SocketStub* old_sock = s.sock;
  const int posts_before = old_sock->events_calls;

  s.install();  // reconfigure: the old session is superseded
  CHECK(session->syncOnce() == SyncOutcome::StaleResult);
  CHECK(old_sock->events_calls == posts_before);  // no new request from a dead session
  CHECK(s.sock->events_calls == 0);               // the new session sent nothing
  CHECK(s.coord.pendingCount() == pending);       // nothing written
  return true;
}

// 8) a reconfigure WHILE the events request is in flight
static bool run_case_reconfigure_while_in_flight() {
  Scenario s;
  s.install();
  const std::shared_ptr<SessionT> session = s.holder.borrow();
  CHECK(session->pullToday());
  CHECK(s.startTask());
  const int pending = s.coord.pendingCount();
  const int64_t generation_before = s.coord.generation();
  SocketStub* old_sock = s.sock;

  s.sock->handler = [&](const std::string&, const std::string& url,
                        const std::string&) {
    if (url.find("events/batch") != std::string::npos) {
      s.install();  // reconfigures exactly while this request is in flight
      return rsp(200, batchBody(pending, {"ev-1", "ev-2"}, "Accepted"));
    }
    if (url.find("challenge") != std::string::npos) return rsp(200, kChallenge);
    if (url.find("/devices/auth") != std::string::npos) return rsp(200, kAuth);
    return rsp(200, todayBody("task-1", "in_progress", 3));
  };

  const auto outcome = session->syncOnce();
  CHECK(old_sock->events_calls == 1);               // the request did go out
  CHECK(s.coord.generation() > generation_before);  // the reconfigure happened
  CHECK(outcome == SyncOutcome::StaleResult);       // late result refused
  CHECK(s.coord.pendingCount() == pending);         // new session state untouched
  CHECK(s.coord.lastAcked() == 0);
  CHECK(session->superseded());
  return true;
}

static bool run_case_all() {
  CASE(authenticate);
  CASE(get_today);
  CASE(offline_terminal_merge);
  CASE(post_events_ack);
  CASE(401_one_reauth);
  CASE(duplicate_lost_response);
  CASE(stale_generation);
  CASE(reconfigure_while_in_flight);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 RF6 backend session orchestration gate ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
