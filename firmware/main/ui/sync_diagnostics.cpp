// CODEX-APP-FIRST-001 AF3b — sync diagnostics presenter.
#include "ui/sync_diagnostics.h"

#include <string>

namespace claw4 {
namespace ui {

SyncDiagnosticView buildSyncDiagnostics(const SyncDiagnosticInput& input) {
  SyncDiagnosticView out;
  out.build_id = input.build_id.empty() ? "unknown" : input.build_id;
  out.network = input.network_online ? "online" : "offline";
  out.auth = input.auth_paused ? "paused" : "ready";
  out.pending = std::to_string(input.pending_count);
  out.last_ack = std::to_string(input.last_acked_sequence);
  out.last_error = input.last_error_code.empty() ? "none" : input.last_error_code;
  out.last_sync = input.last_sync_epoch <= 0
                      ? "never"
                      : std::to_string(input.last_sync_epoch);
  return out;
}

}  // namespace ui
}  // namespace claw4
