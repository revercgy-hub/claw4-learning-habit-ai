// claw4/firmware/tests/unit/sync/single_flight_http_transport_tests.cpp
// WB-V53-NEXT-001 CP1 (A03) — host tests for the exclusive-client HTTP
// boundary. Deterministic latch-based concurrency (no sleeps).

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "sync/single_flight_http_transport.h"

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

static HttpResponse okWith(const std::string& body) {
  HttpResponse r;
  r.transport_ok = true;
  r.status = 200;
  r.body = body;
  return r;
}

static const std::vector<std::pair<std::string, std::string>> kNoHeaders{};

// ---------------------------------------------------------------------------
static bool run_case_runs_inline_and_returns_response() {
  std::string seen_method;
  std::string seen_url;
  std::string seen_body;
  int64_t seen_timeout = -1;
  SingleFlightHttpTransport transport(
      [&](const std::string& method, const std::string& url,
          const std::vector<std::pair<std::string, std::string>>&,
          const std::string& body, int64_t timeout_ms) {
        seen_method = method;
        seen_url = url;
        seen_body = body;
        seen_timeout = timeout_ms;
        return okWith("{}");
      });

  const HttpResponse response =
      transport.request("POST", "http://127.0.0.1/x", kNoHeaders, "{\"a\":1}", 3000);
  CHECK(response.transport_ok);
  CHECK(response.status == 200);
  CHECK(seen_method == "POST");
  CHECK(seen_url == "http://127.0.0.1/x");
  CHECK(seen_body == "{\"a\":1}");
  CHECK(seen_timeout == 3000);
  CHECK(transport.rejectedBusy() == 0);
  CHECK(!transport.inFlight());
  return true;
}

static bool run_case_rejects_concurrent_second_request() {
  std::mutex gate_mutex;
  std::condition_variable gate_cv;
  bool entered = false;
  bool release = false;

  SingleFlightHttpTransport transport(
      [&](const std::string&, const std::string&,
          const std::vector<std::pair<std::string, std::string>>&,
          const std::string&, int64_t) {
        std::unique_lock<std::mutex> lock(gate_mutex);
        entered = true;
        gate_cv.notify_all();
        gate_cv.wait(lock, [&] { return release; });
        return okWith("first");
      });

  HttpResponse first;
  std::thread worker([&] { first = transport.request("GET", "u", kNoHeaders, "", 1000); });

  // Wait until the first request is genuinely inside the blocking call.
  {
    std::unique_lock<std::mutex> lock(gate_mutex);
    gate_cv.wait(lock, [&] { return entered; });
  }
  CHECK(transport.inFlight());

  // A second caller must be rejected immediately instead of racing the client.
  const HttpResponse second = transport.request("GET", "u", kNoHeaders, "", 1000);
  CHECK(!second.transport_ok);   // -> SyncErrorClass::Network -> bounded Backoff
  CHECK(second.status == 0);
  CHECK(transport.rejectedBusy() == 1);

  // And a third, still bounded.
  const HttpResponse third = transport.request("GET", "u", kNoHeaders, "", 1000);
  CHECK(!third.transport_ok);
  CHECK(transport.rejectedBusy() == 2);

  {
    std::lock_guard<std::mutex> lock(gate_mutex);
    release = true;
  }
  gate_cv.notify_all();
  worker.join();

  CHECK(first.transport_ok);
  CHECK(first.body == "first");
  CHECK(!transport.inFlight());   // in-flight flag cleared for the next cycle
  return true;
}

static bool run_case_reentrant_call_is_rejected() {
  int nested_ok = -1;
  SingleFlightHttpTransport* self = nullptr;
  SingleFlightHttpTransport transport(
      [&](const std::string&, const std::string&,
          const std::vector<std::pair<std::string, std::string>>&,
          const std::string&, int64_t) {
        // Re-entrancy from inside the same task is the same hazard as a second
        // concurrent caller and must be refused.
        const HttpResponse nested =
            self->request("GET", "u", kNoHeaders, "", 1000);
        nested_ok = nested.transport_ok ? 1 : 0;
        return okWith("outer");
      });
  self = &transport;

  const HttpResponse outer = transport.request("GET", "u", kNoHeaders, "", 1000);
  CHECK(nested_ok == 0);          // nested call refused
  CHECK(outer.transport_ok);      // outer call unaffected
  CHECK(transport.rejectedBusy() == 1);
  return true;
}

static bool run_case_sequential_requests_are_not_rejected() {
  std::atomic<int> calls{0};
  SingleFlightHttpTransport transport(
      [&](const std::string&, const std::string&,
          const std::vector<std::pair<std::string, std::string>>&,
          const std::string&, int64_t) {
        ++calls;
        return okWith("ok");
      });

  for (int i = 0; i < 5; ++i) {
    const HttpResponse r = transport.request("GET", "u", kNoHeaders, "", 1000);
    CHECK(r.transport_ok);
  }
  CHECK(calls.load() == 5);
  CHECK(transport.rejectedBusy() == 0);
  return true;
}

static bool run_case_invalid_inputs_are_refused() {
  int calls = 0;
  SingleFlightHttpTransport transport(
      [&](const std::string&, const std::string&,
          const std::vector<std::pair<std::string, std::string>>&,
          const std::string&, int64_t) {
        ++calls;
        return okWith("ok");
      });

  // Negative timeout is a programming error; refuse without touching the net.
  const HttpResponse negative = transport.request("GET", "u", kNoHeaders, "", -1);
  CHECK(!negative.transport_ok);
  CHECK(calls == 0);
  CHECK(transport.rejectedBusy() == 0);  // not counted as a busy rejection
  return true;
}

static bool run_case_empty_request_fn_is_refused() {
  SingleFlightHttpTransport transport(nullptr);
  const HttpResponse r = transport.request("GET", "u", kNoHeaders, "", 1000);
  CHECK(!r.transport_ok);
  CHECK(transport.rejectedBusy() == 0);
  return true;
}

static bool run_case_all() {
  CASE(runs_inline_and_returns_response);
  CASE(rejects_concurrent_second_request);
  CASE(reentrant_call_is_rejected);
  CASE(sequential_requests_are_not_rejected);
  CASE(invalid_inputs_are_refused);
  CASE(empty_request_fn_is_refused);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-V53-NEXT-001 CP1 single-flight transport (A03) tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
