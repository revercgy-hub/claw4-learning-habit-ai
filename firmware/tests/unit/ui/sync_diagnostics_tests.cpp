// CODEX-APP-FIRST-001 AF3b — deterministic diagnostics view tests.
#include <cstdio>

#include "ui/sync_diagnostics.h"

namespace {
int g_fail = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    if (!(x)) {                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);         \
      ++g_fail;                                                         \
    }                                                                   \
  } while (0)
}

int main() {
  claw4::ui::SyncDiagnosticInput input;
  input.build_id = "app-first-39d3130";
  input.network_online = true;
  input.auth_paused = true;
  input.pending_count = 3;
  input.last_acked_sequence = 12;
  input.last_error_code = "AUTH_PAUSED";
  input.last_sync_epoch = 1700000000;
  const auto view = claw4::ui::buildSyncDiagnostics(input);
  CHECK(view.build_id == "app-first-39d3130");
  CHECK(view.network == "online");
  CHECK(view.auth == "paused");
  CHECK(view.pending == "3");
  CHECK(view.last_ack == "12");
  CHECK(view.last_error == "AUTH_PAUSED");
  CHECK(view.last_sync == "1700000000");

  const auto empty = claw4::ui::buildSyncDiagnostics({});
  CHECK(empty.build_id == "unknown");
  CHECK(empty.network == "offline");
  CHECK(empty.auth == "ready");
  CHECK(empty.last_error == "none");
  CHECK(empty.last_sync == "never");
  if (g_fail == 0) {
    std::printf("sync_diagnostics_tests: all PASS\n");
    return 0;
  }
  std::printf("sync_diagnostics_tests: %d FAILURES\n", g_fail);
  return 1;
}
