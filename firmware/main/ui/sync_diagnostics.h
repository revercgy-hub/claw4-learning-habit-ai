// CODEX-APP-FIRST-001 AF3b — host-testable sync diagnostics view model.
// The device shell may render these strings later; this layer has no LVGL or
// platform dependency and never invents hardware state.
#pragma once

#include <cstdint>
#include <string>

namespace claw4 {
namespace ui {

struct SyncDiagnosticInput {
  std::string build_id;
  bool network_online = false;
  bool auth_paused = false;
  int pending_count = 0;
  int64_t last_acked_sequence = 0;
  std::string last_error_code;
  int64_t last_sync_epoch = 0;
};

struct SyncDiagnosticView {
  std::string build_id;
  std::string network;
  std::string auth;
  std::string pending;
  std::string last_ack;
  std::string last_error;
  std::string last_sync;
};

SyncDiagnosticView buildSyncDiagnostics(const SyncDiagnosticInput& input);

}  // namespace ui
}  // namespace claw4
