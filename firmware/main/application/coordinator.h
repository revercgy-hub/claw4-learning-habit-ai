// claw4/firmware/main/application/coordinator.h
// Host application coordinator (WB-STREAM-002 CP3).
// Portable C++17. No LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / BSP includes.
//
// Wires the pure reducer, the transactional outbox and the SyncClient boundary
// into a single deterministic application flow:
//   intent -> reducer drafts -> outbox commit -> publish (only after commit)
//   today-task cache merged by (task_id, version) without touching a running
//     active session
//   sync batches only the pending consecutive prefix and applies per-event
//     Accepted/Duplicate + consecutive ACK cleanup
//   auth failures trigger at most one injected re-auth, then pause sync with
//     pending untouched; network/5xx use an injected deterministic backoff
//     capped at 60 s (never sleeps)
//   conflict/rejected/gap / business 4xx / lost & duplicate responses follow
//     the architecture; diagnostics never re-enter the business queue
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "learning_domain/domain_state.h"
#include "learning_domain/event.h"
#include "learning_domain/intents.h"
#include "learning_domain/reducer.h"
#include "sync/outbox_core.h"
#include "sync/sync_client.h"

namespace claw4 {
namespace application {

// Injected transport boundary (real HTTP/TLS is a later device-network task).
class SyncTransport {
 public:
  virtual ~SyncTransport() = default;
  virtual sync::SyncClient::Response send(const sync::SyncClient::Request& request) = 0;
};

// Injected credential refresh: called at most once per auth failure. Returns
// true when re-auth succeeded (the caller may retry the sync).
using ReauthFn = std::function<bool()>;

struct CoordinatorOptions {
  int64_t backoff_base_ms = 1000;
  int64_t backoff_max_ms = 60000;   // 60 s cap (ARCHITECTURE.md §7.5)
  int64_t jitter_seed = 42;         // deterministic jitter (no RNG in tests)
  int64_t jitter_amplitude_ms = 50; // +/- range
};

enum class SyncOutcome : uint8_t {
  Synced = 0,        // response processed; see lastSyncResult
  ReauthOk,          // auth failed once, injected re-auth succeeded; retry sync
  PausedAuth,        // auth failed and re-auth failed; sync paused, pending kept
  Backoff,           // network/5xx; retry after backoffDelayMs()
  NoPending,         // nothing to sync
};

class AppCoordinator {
 public:
  AppCoordinator(sync::OutboxStorage& storage,
                 const domain::DomainReducer& reducer,
                 const CoordinatorOptions& options = {});

  // --- domain entry -----------------------------------------------------
  // Reducer -> outbox commit -> publish. Publishes ONLY after the outbox
  // committed; on reducer rejection or outbox failure the caller/UI never sees
  // an uncommitted state.
  domain::TransitionResult dispatchIntent(const domain::IntentRequest& intent,
                                          const domain::ReducerContext& ctx);

  // Focus-segment timeout through the same commit-then-publish pipeline.
  domain::TransitionResult dispatchSegmentTimeout(const domain::SessionId& session_id,
                                                  const domain::ReducerContext& ctx);

  // Retries the last intent whose outbox commit failed, reusing the SAME
  // event_id values (ARCHITECTURE.md §7.3.1: retry never generates new ids).
  domain::TransitionResult retryLastFailed();

  // --- today-task cache -------------------------------------------------
  // Incremental merge by (task_id, version). Never overwrites the status of a
  // task that owns the running active session; an empty list is a normal
  // "no tasks today" state. The merged cache is committed through the outbox
  // so it survives reboot.
  bool mergeTodayTasks(const std::vector<domain::Task>& server_tasks);

  // --- sync -------------------------------------------------------------
  // Sends only the pending consecutive prefix (sequence == last_acked+1 ...).
  // Never sleeps; returns an outcome and (when Backoff) the suggested delay.
  SyncOutcome runSyncOnce(SyncTransport& transport, const ReauthFn& reauth);

  // --- accessors --------------------------------------------------------
  const domain::DomainState& state() const { return state_; }
  int64_t backoffDelayMs() const { return pending_backoff_ms_; }
  int retryCount() const { return retry_count_; }
  int pendingCount() const;
  int64_t lastAcked() const;

 private:
  // Shared commit-then-publish pipeline used by both entry points.
  domain::TransitionResult commitAndPublish(domain::TransitionResult r);
  domain::TransitionResult persistOrFail(sync::PendingTransition t);
  bool reloadState();
  sync::OutboxStorage& storage_;
  sync::OutboxCore outbox_;
  const domain::DomainReducer& reducer_;
  const CoordinatorOptions options_;
  domain::DomainState state_;   // published state (committed only)
  // Kept when an outbox commit failed so a retry reuses the same event_ids.
  std::optional<sync::PendingTransition> last_failed_;
  int retry_count_ = 0;
  int64_t pending_backoff_ms_ = 0;
  bool reauth_attempted_ = false;  // one re-auth attempt per sync pause
  sync::SyncClient::Response last_sync_response_;
};

}  // namespace application
}  // namespace claw4
