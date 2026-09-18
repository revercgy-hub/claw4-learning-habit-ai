// A05-DEVICE-T1 diagnostic (LOCAL ONLY, not part of the repo).
//
// Host-side REPRODUCTION of the ACK_PIPELINE_FAIL defect, using:
//   * the REAL /api/v1/events/batch response bytes the device actually received
//     (decoded by the device's own wire::DecodeBatchResponse), and
//   * the REAL AppCoordinator + OutboxCore + FakeOutboxStorage (the same
//     implementation the host gate compiles).
//
// Phase 1 reproduces the defect deterministically on the host.
// Phase 2 is a SIMULATED counterfactual for the proposed fix (fresh sequence
// range) -- it uses the fake's "everything accepted" server behaviour, so it is
// only indicative. Real validation needs a scratch backend (see the fix
// proposal document).
//
// Build (see the fix proposal / T1 finding doc for the full recipe):
//   g++ -std=c++17 -Wall -Wextra -I firmware/main -I firmware/tests -I integration
//       -c _t1_host_repro.cpp -o repro.o
//   g++ repro.o <repo>/out/<gate>/common-*.o -o repro.exe
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "application/coordinator.h"
#include "fakes/fake_outbox_storage.h"
#include "fakes/fake_sync_transport.h"
#include "learning_domain/reducer.h"
#include "sync/wire_codec.h"

using claw4::application::AppCoordinator;
using claw4::application::CoordinatorOptions;
using claw4::application::SyncOutcome;
using claw4::domain::ChildId;
using claw4::domain::DeviceId;
using claw4::domain::DomainReducer;
using claw4::domain::EventId;
using claw4::sync::FakeDisk;
using claw4::sync::FakeOutboxStorage;
using claw4::fakes::FakeSyncTransport;
using claw4::sync::PendingEvent;
using claw4::sync::SyncClient;

static int g_fail = 0;

static void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++g_fail;
}

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);  // unbuffered: survive a hard crash
  const char* resp_path =
      (argc > 1) ? argv[1] : "E:/workbuddy/_t1_resp_actual.json";
  std::printf("start argv=%d\n", argc);

  std::ifstream in(resp_path, std::ios::binary);
  if (!in) {
    std::printf("cannot open %s\n", resp_path);
    return 2;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string json = ss.str();

  SyncClient::Response real;
  if (!claw4::sync::wire::DecodeBatchResponse(json, real)) {
    std::printf("DecodeBatchResponse FAILED on %s\n", resp_path);
    return 2;
  }
  std::printf("REAL wire response: bytes=%zu last_acked=%lld results=%zu\n",
              json.size(),
              static_cast<long long>(real.batch.last_acked_sequence),
              real.batch.results.size());

  // ---------------------------------------------------------------------
  // seed the device outbox with the REAL pending rows (sequence/event_id taken
  // verbatim from the device's own request, captured on the wire)
  // ---------------------------------------------------------------------
  auto disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};

  auto seed = [&](int64_t seq, const char* id) {
    PendingEvent p;
    p.event_id = EventId{id};
    p.device_id = DeviceId{"dev-real"};
    p.child_id = ChildId{"child-1"};
    p.sequence = seq;
    p.timestamp = 1758000000 + seq;
    p.timestamp_source = claw4::domain::TimestampSource::Local;
    p.version = 1;  // type/payload are irrelevant to the ACK scope walk
    disk->state.pending.push_back(p);
  };

  seed(1, "ev-58171c750ba9486d");
  seed(2, "ev-dc9f5b28365fee5b");
  seed(3, "ev-92598e1485b898e6");
  seed(4, "ev-11b9439b49b66de0");
  seed(5, "ev-5b04b20bc0628a3b");
  seed(6, "ev-f1b86c5e7c505961");
  seed(7, "ev-effc25824dd81a3b");
  seed(8, "ev-c46b388b758545ad");
  seed(9, "ev-3c141ce6bf0d75d7");
  seed(10, "ev-7e628978f9afae32");
  seed(11, "ev-73f665b4b7037ee6");
  seed(12, "ev-44b8834561325096");
  seed(13, "ev-ca6e1f52fb88c2e8");
  seed(14, "ev-f2a8f3d60076eb1a");
  seed(15, "ev-fbe83fa4e8c92d6b");
  seed(16, "ev-8173c21c3c514311");
  seed(17, "ev-5c58eefbbc384347");
  seed(18, "ev-85f1bce5aca13b28");
  seed(19, "ev-5f36f70922ef3033");
  seed(20, "ev-ac93c5c0568ec8c3");

  disk->state.last_acked_sequence = 0;  // A_local = 0  (NVS: A|0)
  disk->state.next_sequence = 21;       // NVS: Q|21

  DomainReducer reducer;
  CoordinatorOptions opts;
  AppCoordinator coord{storage, reducer, opts};

  FakeSyncTransport transport;
  transport.on_send = [&real]() { return real; };

  // =====================================================================
  std::printf("\n===== PHASE 1: reproduce with REAL wire bytes (frozen candidate) =====\n");
  const SyncOutcome outcome = coord.runSyncOnce(transport, nullptr);

  std::printf("  runSyncOnce outcome     = %d (0=Synced)\n", static_cast<int>(outcome));
  std::printf("  requests sent           = %zu\n", transport.requests.size());
  if (!transport.requests.empty()) {
    const auto& rq = transport.requests[0];
    std::printf("  request last_acked      = %lld\n",
                static_cast<long long>(rq.last_acked_sequence));
    std::printf("  request events          = %zu\n", rq.events.size());
  }
  std::printf("  removeAcked called      = %d time(s)\n", disk->ack_calls);
  std::printf("  pending after           = %zu\n", disk->state.pending.size());
  std::printf("  last_acked after        = %lld\n",
              static_cast<long long>(disk->state.last_acked_sequence));
  std::printf("  diagnostic sync_failed  = %d  sync_recovered = %d\n",
              disk->state.diagnostic.sync_failed ? 1 : 0,
              disk->state.diagnostic.sync_recovered ? 1 : 0);

  check(transport.requests.size() == 1 && transport.requests[0].events.size() == 20,
        "sent the consecutive prefix: 20 events");
  check(transport.requests[0].last_acked_sequence == 0,
        "request carried the device's own last_acked = 0");
  check(disk->ack_calls == 0,
        "DEFECT reproduced: removeAcked() was NEVER called");
  check(disk->state.pending.size() == 20,
        "DEFECT reproduced: all 20 rows still pending");
  check(disk->state.last_acked_sequence == 0,
        "DEFECT reproduced: local ACK did not advance");
  check(outcome == SyncOutcome::Synced,
        "DEFECT reproduced: reported Synced (false success)");

  // =====================================================================
  std::printf("\n===== PHASE 2: SIMULATED counterfactual for the proposed fix =====\n");
  std::printf("  (renumber pending to fresh slots above the server baseline,\n");
  std::printf("   then a normal flush -- server behaviour simulated by the fake)\n");
  int64_t next = real.batch.last_acked_sequence;  // = 21
  for (auto& p : disk->state.pending) {
    p.sequence = ++next;
  }
  disk->state.last_acked_sequence = real.batch.last_acked_sequence;
  disk->state.next_sequence = next + 1;

  transport.requests.clear();
  // Fresh transport with the fake's default behaviour (empty results ->
  // "everything accepted, ACK advanced to the last sent sequence"). Clearly a
  // SIMULATED server: real validation needs the scratch backend.
  FakeSyncTransport transport2;
  const SyncOutcome outcome2 = coord.runSyncOnce(transport2, nullptr);

  std::printf("  runSyncOnce outcome     = %d (0=Synced)\n", static_cast<int>(outcome2));
  std::printf("  removeAcked called      = %d time(s)\n", disk->ack_calls);
  std::printf("  pending after           = %zu\n", disk->state.pending.size());
  std::printf("  last_acked after        = %lld\n",
              static_cast<long long>(disk->state.last_acked_sequence));

  check(disk->ack_calls == 1, "SIMULATED fix: removeAcked() called once");
  check(disk->state.pending.empty(), "SIMULATED fix: queue drained (removed > 0)");
  check(disk->state.last_acked_sequence == 41,
        "SIMULATED fix: local ACK advanced to 41");

  std::printf("\nresult: %s (%d failed assertion(s))\n",
              g_fail == 0 ? "AS EXPECTED" : "UNEXPECTED", g_fail);
  return g_fail == 0 ? 0 : 1;
}
