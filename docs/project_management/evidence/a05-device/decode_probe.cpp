// A05-DEVICE-T1 diagnostic (LOCAL ONLY, not part of the repo).
// Feeds a REAL /api/v1/events/batch response body into the device's own wire
// decoder to decide whether the frozen candidate can parse what the backend
// actually returns.
//
// Build:
//   g++ -std=c++17 -I <repo>/firmware/main <repo>/firmware/main/sync/wire_codec.cpp \
//       _t1_decode_probe.cpp -o _t1_decode_probe.exe
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "sync/wire_codec.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: _t1_decode_probe <batch_response.json>\n");
    return 2;
  }
  std::ifstream in(argv[1], std::ios::binary);
  if (!in) {
    std::printf("cannot open %s\n", argv[1]);
    return 2;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string json = ss.str();
  std::printf("input = %s (%zu bytes)\n", argv[1], json.size());

  claw4::sync::SyncClient::Response resp;
  const bool ok = claw4::sync::wire::DecodeBatchResponse(json, resp);
  std::printf("DecodeBatchResponse = %s\n", ok ? "TRUE" : "FALSE");
  if (!ok) {
    return 1;
  }
  std::printf("  error_class     = %d\n", static_cast<int>(resp.error_class));
  std::printf("  http_status     = %d\n", resp.http_status);
  std::printf("  last_acked      = %lld\n",
              static_cast<long long>(resp.batch.last_acked_sequence));
  std::printf("  server_time     = %lld\n",
              static_cast<long long>(resp.batch.server_time));
  std::printf("  results         = %zu\n", resp.batch.results.size());
  for (const auto& r : resp.batch.results) {
    std::printf("    seq=%-4lld outcome=%d http=%d id=%s eligible_for_ack=%s\n",
                static_cast<long long>(r.sequence), static_cast<int>(r.outcome),
                r.http_status, r.event_id.value.c_str(),
                (r.sequence <= resp.batch.last_acked_sequence &&
                 (r.outcome == claw4::sync::EventOutcome::Accepted ||
                  r.outcome == claw4::sync::EventOutcome::Duplicate))
                    ? "YES" : "no");
  }
  return 0;
}
