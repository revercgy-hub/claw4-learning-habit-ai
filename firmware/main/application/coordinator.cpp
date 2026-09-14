// claw4/firmware/main/application/coordinator.cpp
// Host application coordinator implementation (WB-STREAM-002 CP3).
#include "application/coordinator.h"

#include <algorithm>
#include <cmath>

namespace claw4 {
namespace application {

namespace {
sync::OutboxState loadFrom(sync::OutboxStorage& storage) {
  sync::OutboxState s;
  storage.load(s);
  return s;
}

bool sameTask(const domain::Task& a, const domain::Task& b) {
  return a.task_id == b.task_id && a.child_id == b.child_id &&
         a.title == b.title && a.subject == b.subject &&
         a.estimated_minutes == b.estimated_minutes &&
         a.status == b.status && a.version == b.version;
}

bool sameSession(const domain::StudySession& a, const domain::StudySession& b) {
  return a.session_id == b.session_id && a.task_id == b.task_id &&
         a.child_id == b.child_id && a.device_id == b.device_id &&
         a.planned_minutes == b.planned_minutes &&
         a.actual_seconds == b.actual_seconds && a.pause_count == b.pause_count &&
         a.pause_seconds == b.pause_seconds && a.status == b.status &&
         a.completion_type == b.completion_type &&
         a.segment_start_monotonic_ms == b.segment_start_monotonic_ms &&
         a.paused_at_monotonic_ms == b.paused_at_monotonic_ms;
}

// Structural equality used to detect "no visible change" transitions such as
// intact-snapshot recovery (value types lack operator== on purpose).
bool sameDomainState(const domain::DomainState& a, const domain::DomainState& b) {
  if (a.device_state != b.device_state ||
      a.pending_event_count != b.pending_event_count ||
      a.last_acked_sequence != b.last_acked_sequence ||
      a.time_synced != b.time_synced || a.tasks.size() != b.tasks.size() ||
      a.active_session.has_value() != b.active_session.has_value()) {
    return false;
  }
  for (std::size_t i = 0; i < a.tasks.size(); ++i) {
    if (!sameTask(a.tasks[i], b.tasks[i])) return false;
  }
  if (a.active_session.has_value() &&
      !sameSession(*a.active_session, *b.active_session)) {
    return false;
  }
  return true;
}
}  // namespace

AppCoordinator::AppCoordinator(sync::OutboxStorage& storage,
                               const domain::DomainReducer& reducer,
                               const CoordinatorOptions& options)
    : storage_(storage), outbox_(&storage), reducer_(reducer), options_(options) {
  reloadState();
}

bool AppCoordinator::reloadState() {
  state_ = loadFrom(storage_).domain;
  return true;
}

int AppCoordinator::pendingCount() const { return outbox_.pendingCount(); }
int64_t AppCoordinator::lastAcked() const { return outbox_.lastAcked(); }

bool AppCoordinator::prepareAfterBoot(int64_t monotonic_ms) {
  if (monotonic_ms < 0) return false;
  sync::OutboxState stored;
  if (!storage_.load(stored)) return false;
  auto next = stored.domain;
  if (next.active_session) {
    auto& session = *next.active_session;
    session.segment_start_monotonic_ms =
        session.status == domain::SessionStatus::Running ? monotonic_ms : 0;
    session.paused_at_monotonic_ms =
        session.status == domain::SessionStatus::Paused ? monotonic_ms : 0;
    sync::PendingTransition transition;
    transition.next_state = next;
    if (!outbox_.persistTransition(transition).committed()) return false;
  }
  state_ = std::move(next);
  return true;
}

domain::TransitionResult AppCoordinator::dispatchIntent(
    const domain::IntentRequest& intent, const domain::ReducerContext& ctx) {
  return commitAndPublish(reducer_.reduce(state_, intent, ctx));
}

domain::TransitionResult AppCoordinator::dispatchSegmentTimeout(
    const domain::SessionId& session_id, const domain::ReducerContext& ctx) {
  if (!state_.active_session) {
    domain::TransitionResult fail;
    fail.ok = false;
    fail.intent_result = domain::IntentResult::RejectedInvalidState;
    fail.reason = domain::RejectReason::SessionNotActive;
    return fail;
  }
  return commitAndPublish(
      reducer_.onSegmentTimeout(state_, session_id, ctx));
}

domain::TransitionResult AppCoordinator::commitAndPublish(domain::TransitionResult r) {
  if (!r.ok) {
    // Rejection / idempotent: nothing is committed, nothing is published.
    return r;
  }
  const bool unchanged = !r.next.has_value() ||
                         (sameDomainState(*r.next, state_) && r.drafts.empty());
  if (unchanged) {
    // No change (e.g. intact-snapshot recovery): nothing to commit.
    return r;
  }
  sync::PendingTransition t;
  t.next_state = *r.next;
  t.event_drafts = std::move(r.drafts);
  return persistOrFail(std::move(t));
}

domain::TransitionResult AppCoordinator::retryLastFailed() {
  if (!last_failed_) {
    domain::TransitionResult fail;
    fail.ok = false;
    fail.intent_result = domain::IntentResult::RejectedInvalidState;
    fail.reason = domain::RejectReason::None;
    return fail;
  }
  return persistOrFail(*last_failed_);
}

domain::TransitionResult AppCoordinator::persistOrFail(sync::PendingTransition t) {
  sync::PersistResult pr = outbox_.persistTransition(t);
  if (!pr.committed()) {
    // Keep the failed transition so a retry reuses the SAME event_ids.
    last_failed_ = std::move(t);
    domain::TransitionResult fail;
    fail.ok = false;
    fail.intent_result = domain::IntentResult::PersistFailed;
    fail.reason = domain::RejectReason::None;
    return fail;
  }
  last_failed_.reset();
  // Publish only the committed next state.
  state_ = t.next_state;
  domain::TransitionResult ok;
  ok.ok = true;
  ok.intent_result = domain::IntentResult::Accepted;
  ok.next = t.next_state;
  ok.drafts.clear();  // consumed by the outbox (see committed_events in result)
  return ok;
}

bool AppCoordinator::applyTodaySnapshot(
    const std::vector<domain::Task>& server_tasks) {
  sync::OutboxState os = loadFrom(storage_);
  domain::DomainState next = os.domain;

  // The task owning a live Running/Paused session is the ONLY row protected
  // from snapshot membership: it is kept (until the session ends) even when
  // the server does not list it, and its live status is never reset.
  const domain::TaskId* active_id = nullptr;
  if (next.active_session.has_value() &&
      (next.active_session->status == domain::SessionStatus::Running ||
       next.active_session->status == domain::SessionStatus::Paused)) {
    active_id = &next.active_session->task_id;
  }

  std::vector<domain::Task> out_tasks;
  out_tasks.reserve(next.tasks.size() + server_tasks.size());

  for (auto& local : next.tasks) {
    if (active_id && *active_id == local.task_id) {
      // Active task: keep it; apply newer descriptive fields only, never the
      // server's status (the reducer owns the live status).
      for (const auto& sv : server_tasks) {
        if (sv.task_id == local.task_id && sv.version >= local.version) {
          const domain::TaskStatus live_status = local.status;
          local = sv;
          local.status = live_status;  // keep InProgress/Paused
        }
      }
      out_tasks.push_back(local);
      continue;
    }
    // Non-active task: authoritative membership — drop when absent from the
    // server snapshot; keep when present (honouring the version guard).
    for (const auto& sv : server_tasks) {
      if (sv.task_id == local.task_id) {
        out_tasks.push_back(sv.version >= local.version ? sv : local);
        break;
      }
    }
    // absent from the server -> removed (cache row no longer authoritative)
  }

  // New tasks (present on the server, unknown locally).
  for (const auto& sv : server_tasks) {
    bool exists = false;
    for (const auto& t : out_tasks) {
      if (t.task_id == sv.task_id) {
        exists = true;
        break;
      }
    }
    if (!exists) out_tasks.push_back(sv);
  }
  next.tasks = std::move(out_tasks);

  // Persist the snapshot through the outbox (empty drafts: snapshot-only
  // commit) so it survives reboot.
  sync::PendingTransition t;
  t.next_state = next;
  sync::PersistResult pr = outbox_.persistTransition(t);
  if (!pr.committed()) return false;
  state_ = next;
  return true;
}

// A03: clamp a server batch result to the sequences this exchange really sent.
sync::BatchSyncResult AppCoordinator::scopeBatchToSent(
    const SyncRequestEnvelope& envelope, const sync::BatchSyncResult& batch) {
  sync::BatchSyncResult scoped;
  // The advertised consecutive prefix can never exceed what was sent.
  scoped.last_acked_sequence =
      std::min(batch.last_acked_sequence, envelope.max_sent_sequence);
  scoped.server_time = batch.server_time;
  scoped.results.reserve(batch.results.size());
  for (const auto& row : batch.results) {
    // A result may only describe an event from THIS request's consecutive
    // prefix. Rows below it are already ACKed; rows above it were never sent,
    // so an out-of-range or replayed ACK can never delete pending events that
    // were queued after prepareSync() captured the envelope.
    if (row.sequence < envelope.first_sent_sequence) continue;
    if (row.sequence > envelope.max_sent_sequence) continue;
    scoped.results.push_back(row);
  }
  return scoped;
}

SyncRequestEnvelope AppCoordinator::prepareSync() const {
  SyncRequestEnvelope env;
  if (auth_paused_) {
    env.auth_paused = true;
    return env;
  }

  const sync::OutboxState os = loadFrom(storage_);
  if (os.pending.empty()) return env;  // has_work stays false -> NoPending

  env.has_work = true;
  env.generation = generation_;
  env.request.device_id = state_.active_session.has_value()
                              ? state_.active_session->device_id
                              : os.pending.front().device_id;
  env.request.last_acked_sequence = os.last_acked_sequence;
  env.first_sent_sequence = os.last_acked_sequence + 1;

  int64_t expected = os.last_acked_sequence + 1;
  for (const auto& row : os.pending) {
    if (row.sequence != expected) break;  // gap: stop at the first discontinuity
    claw4::domain::DeviceEvent ev;
    ev.event_id = row.event_id;
    ev.device_id = row.device_id;
    ev.child_id = row.child_id;
    ev.sequence = row.sequence;
    ev.timestamp = row.timestamp;
    ev.timestamp_source = row.timestamp_source;
    ev.type = row.type;
    ev.version = row.version;
    ev.payload = row.payload;
    env.request.events.push_back(ev);
    expected = row.sequence + 1;
  }
  // When the queue head is not the next consecutive sequence the prefix is
  // legitimately empty and max_sent_sequence stays at last_acked, so the scope
  // guard refuses every ACK.
  env.max_sent_sequence = expected - 1;
  return env;
}

SyncApplyOutcome AppCoordinator::applySyncResult(
    const SyncRequestEnvelope& envelope,
    const sync::SyncClient::Response& response) {
  SyncApplyOutcome out;

  // (1) Session ownership: a worker that was in flight while the runtime was
  //     reconfigured, torn down or given a fresh session must not write into
  //     the new session.
  if (envelope.generation != generation_) {
    out.outcome = SyncOutcome::StaleResult;
    out.stale = true;
    out.applied = false;
    return out;
  }

  // (2) Auth: at most ONE re-auth per pause. This function performs no I/O and
  //     never invokes the caller's credential refresh; on the first auth
  //     failure it only reports ReauthOk so the caller can refresh outside the
  //     state lock and then re-run the exchange.
  if (response.error_class == sync::SyncErrorClass::Auth ||
      response.http_status == 401 || response.http_status == 403) {
    if (!reauth_attempted_) {
      reauth_attempted_ = true;
      out.outcome = SyncOutcome::ReauthOk;
      out.applied = false;
      return out;
    }
    auth_paused_ = true;
    outbox_.setDiagnostic(true, false);
    out.outcome = SyncOutcome::PausedAuth;
    out.applied = true;
    return out;
  }

  // (3) Network / 5xx: deterministic backoff capped at 60 s (never sleeps).
  if (response.error_class == sync::SyncErrorClass::Network ||
      response.error_class == sync::SyncErrorClass::Server ||
      response.http_status >= 500) {
    ++retry_count_;
    int64_t base = options_.backoff_base_ms;
    for (int i = 0; i < retry_count_ - 1 && base < options_.backoff_max_ms; ++i) {
      base *= 2;
    }
    base = std::min(base, options_.backoff_max_ms);
    // Deterministic jitter from the injected seed (no RNG).
    const int64_t amp = options_.jitter_amplitude_ms;
    const int64_t jitter =
        ((options_.jitter_seed + retry_count_ * 7919) % (2 * amp + 1)) - amp;
    pending_backoff_ms_ = base + jitter;
    outbox_.setDiagnostic(true, false);
    out.outcome = SyncOutcome::Backoff;
    out.applied = true;
    return out;
  }

  // (4) Scoped ACK cleanup: only rows actually sent AND confirmed inside the
  //     consecutive prefix are removed; anything queued after prepareSync()
  //     keeps its pending row.
  const sync::PersistResult pr =
      outbox_.applyBatchResult(scopeBatchToSent(envelope, response.batch));
  if (!pr.committed()) {
    ++retry_count_;
    pending_backoff_ms_ = options_.backoff_max_ms;  // storage trouble: slow down
    out.outcome = SyncOutcome::Backoff;
    out.applied = false;
    return out;
  }
  // Success path resets backoff/reauth state and marks the diagnostic
  // recovered (mergeable slot; never enters the business queue).
  retry_count_ = 0;
  pending_backoff_ms_ = 0;
  reauth_attempted_ = false;
  auth_paused_ = false;
  outbox_.setDiagnostic(false, true);
  reloadState();
  out.outcome = SyncOutcome::Synced;
  out.applied = true;
  return out;
}

// Legacy synchronous wrapper. Kept byte-for-byte compatible with the pre-CP1
// contract so every existing Host test, the Virtual Device fault matrix and the
// backend wire fixtures keep working unchanged.
SyncOutcome AppCoordinator::runSyncOnce(SyncTransport& transport,
                                        const ReauthFn& reauth) {
  const SyncRequestEnvelope env = prepareSync();
  // FIX-V4-01 auth-pause gate: while paused the transport is short-circuited —
  // no send, no re-auth, pending untouched. Only resetAuthPause() resumes.
  if (env.auth_paused) return SyncOutcome::PausedAuth;
  if (!env.has_work) {
    pending_backoff_ms_ = 0;
    retry_count_ = 0;
    return SyncOutcome::NoPending;
  }

  const sync::SyncClient::Response resp = transport.send(env.request);
  last_sync_response_ = resp;

  const SyncApplyOutcome applied = applySyncResult(env, resp);
  if (applied.outcome == SyncOutcome::ReauthOk) {
    // The credential refresh happens here — deliberately OUTSIDE
    // applySyncResult() — so a device shell can perform it without holding the
    // state lock across the network wait.
    if (reauth && reauth()) return SyncOutcome::ReauthOk;
    reauth_attempted_ = true;  // pause sync until resetAuthPause()
    auth_paused_ = true;       // FIX-V4-01: transport short-circuits now
    outbox_.setDiagnostic(true, false);
    return SyncOutcome::PausedAuth;
  }
  return applied.outcome;
}

void AppCoordinator::beginNewSession() {
  // Any envelope prepared before this point carries the old generation and is
  // rejected by applySyncResult() instead of mutating the new session.
  ++generation_;
  auth_paused_ = false;
  reauth_attempted_ = false;
  pending_backoff_ms_ = 0;
  retry_count_ = 0;
}

void AppCoordinator::resetAuthPause() {
  auth_paused_ = false;
  reauth_attempted_ = false;
}

}  // namespace application
}  // namespace claw4
