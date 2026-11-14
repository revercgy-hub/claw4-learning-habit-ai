#include "application/learning_voice_commands.h"
#include "application/learning_voice_exchange.h"
#include "fakes/fake_outbox_storage.h"

#include <cstdlib>
#include <cstdio>
#include <memory>

using namespace claw4;
using namespace claw4::application;

void checkAt(bool ok, int line) {
  if (ok) return;
  std::fprintf(stderr, "learning voice command assertion failed at line %d\n", line);
  std::exit(EXIT_FAILURE);
}
#define check(expression) checkAt((expression), __LINE__)

class Clock final : public ports::ClockPort {
 public:
  int64_t epochSeconds() override { return 0; }
  int64_t monotonicMs() override { return 10000; }
  bool isTimeSynced() override { return false; }
};

class Store final : public reminder::ReminderStore {
 public:
  std::vector<reminder::ReminderRecord> rows;
  bool fail = false;
  bool load(std::vector<reminder::ReminderRecord>& out) override { out = rows; return true; }
  bool save(const std::vector<reminder::ReminderRecord>& next) override {
    if (fail) return false;
    rows = next;
    return true;
  }
};

int main() {
  constexpr int64_t now = 1756718530000LL;
  auto disk = std::make_shared<sync::FakeDisk>();
  sync::FakeOutboxStorage storage(disk);
  domain::DomainReducer reducer;
  AppCoordinator coordinator(storage, reducer);
  Clock clock;
  time::TimeAuthority authority(clock, "boot-a", {});
  Store store;
  reminder::ReminderCore reminders({}, store, authority);
  LearningVoiceCommands commands(coordinator, reminders);
  using Kind = LearningVoiceKind;
  using Status = LearningVoiceStatus;
  LearningVoiceBinding binding{{"dev"}, {"child"}, 8};
  int ids = 0;
  domain::ReducerContext ctx{
      {"dev"}, {"child"}, now / 1000, 10000,
      [&]() { return domain::EventId{"event-" + std::to_string(++ids)}; },
      []() { return domain::SessionId{"study-1"}; }};
  domain::Task task;
  task.task_id = {"math"};
  task.child_id = {"child"};
  task.status = domain::TaskStatus::Ready;
  task.estimated_minutes = 2;
  task.version = 3;
  auto other = task;
  other.task_id = {"reading"};
  auto foreign = task;
  foreign.task_id = {"foreign"};
  foreign.child_id = {"different-child"};
  check(coordinator.applyTodaySnapshot({task, other, foreign}));

  auto request = [&](Kind kind, std::optional<domain::TaskId> id = std::nullopt) {
    return LearningVoiceRequest{"request-" + std::to_string(++ids), 8, kind, id, {}, now + 30000};
  };
  auto run = [&](const LearningVoiceRequest& r, std::optional<std::string> displayed = std::nullopt) {
    return commands.execute(r, binding, ctx, now, now - 1000, displayed);
  };
  auto query = request(Kind::QueryTodayTasks);
  check(commands.execute(query, {}, ctx, now, now, {}).status == Status::Stale);
  check(commands.execute(query, binding, ctx, now, {}, {}).status == Status::Stale);
  const auto today = run(query);
  check(today.status == Status::Applied && today.tasks.size() == 2 && today.snapshot_epoch_ms == now - 1000);
  check(run(request(Kind::StartTask)).status == Status::AmbiguousTask);
  check(run(request(Kind::StartTask, foreign.task_id)).status == Status::InvalidState);
  auto start = request(Kind::StartTask, task.task_id);
  start.expected_task_version = 2;
  check(run(start).status == Status::Stale);
  start.expected_task_version = 3;
  check(run(start).status == Status::Applied);
  ctx.monotonic_ms += 5000;
  const auto remaining = run(request(Kind::QueryRemainingTime));
  check(remaining.focus && remaining.focus->remaining_ms == 115000);

  const auto complete = request(Kind::CompleteRequest);
  check(run(complete).status == Status::ConfirmationRequired);
  check(run(complete).status == Status::ConfirmationRequired);
  commands.cancelConfirmation();
  check(run(complete).status == Status::Stale);
  check(commands.confirmFromTouch(binding, ctx, now).status == Status::InvalidState);
  check(run(request(Kind::CompleteRequest)).status == Status::ConfirmationRequired);
  check(coordinator.state().active_session.has_value());
  check(run(request(Kind::PauseTask)).status == Status::Applied);
  check(commands.confirmFromTouch(binding, ctx, now).status == Status::Stale);
  check(run(request(Kind::ResumeTask)).status == Status::Applied);
  check(run(request(Kind::CompleteRequest)).status == Status::ConfirmationRequired);
  auto stale = binding;
  ++stale.session_epoch;
  check(commands.confirmFromTouch(stale, ctx, now).status == Status::Stale);
  check(run(request(Kind::CompleteRequest)).status == Status::ConfirmationRequired);
  disk->fail_next_commit = true;
  check(commands.confirmFromTouch(binding, ctx, now).status == Status::StorageError);
  check(coordinator.state().active_session.has_value());
  check(commands.confirmFromTouch(binding, ctx, now).status == Status::Applied);
  check(!coordinator.state().active_session);
  check(commands.confirmFromTouch(binding, ctx, now).status == Status::InvalidState);

  reminder::ReminderRecord r;
  r.child_id = {"child"};
  r.task_id = other.task_id;
  r.kind = reminder::ReminderKind::TaskDue;
  r.due_epoch_ms = now;
  r.instance_id = reminder::ReminderCore::makeInstanceId(r.kind, r.child_id, r.task_id, now);
  check(reminders.upsert(r));
  check(run(request(Kind::SnoozeReminder)).status == Status::InvalidState);
  const auto snooze = request(Kind::SnoozeReminder);
  store.fail = true;
  check(run(snooze, r.instance_id).status == Status::StorageError);
  store.fail = false;
  check(run(snooze, r.instance_id).status == Status::Applied);
  const auto snoozed_until = store.rows.front().snooze_until_epoch_ms;
  check(snoozed_until > now);

  AppCoordinator restarted_coordinator(storage, reducer);
  reminder::ReminderCore restarted_reminders({}, store, authority);
  check(restarted_reminders.load());
  LearningVoiceCommands restarted(restarted_coordinator, restarted_reminders);
  check(restarted.confirmFromTouch(binding, ctx, now).status == Status::InvalidState);
  check(restarted.execute(snooze, binding, ctx, now + 500, now, r.instance_id).status == Status::Applied);
  check(store.rows.front().snooze_until_epoch_ms == snoozed_until);

  // Losing an applied pause reply and replaying it after restart/resume must
  // never pause the new running segment.
  check(run(request(Kind::StartTask, other.task_id)).status == Status::Applied);
  const auto pause = request(Kind::PauseTask);
  disk->fail_next_commit = true;
  check(run(pause).status == Status::StorageError);
  check(coordinator.state().active_session->status == domain::SessionStatus::Running);
  check(run(pause).status == Status::Applied);
  check(run(request(Kind::ResumeTask)).status == Status::Applied);
  AppCoordinator recovered(storage, reducer);
  LearningVoiceCommands recovery(recovered, reminders);
  const auto before = recovered.pendingCount();
  const auto replay = recovery.execute(pause, binding, ctx, now, {}, {});
  check(replay.status == Status::Applied && replay.task_version == other.version);
  check(recovered.pendingCount() == before);
  check(recovered.state().active_session->status == domain::SessionStatus::Running);
  auto collision = pause;
  collision.kind = Kind::ResumeTask;
  check(recovery.execute(collision, binding, ctx, now, {}, {}).status == Status::Stale);
  collision = pause;
  ++collision.expires_at_ms;
  check(recovery.execute(collision, binding, ctx, now, {}, {}).status == Status::Stale);

  const auto later = now + 31'000;
  auto new_pause = request(Kind::PauseTask);
  new_pause.expires_at_ms = later + 30'000;
  check(recovery.execute(new_pause, binding, ctx, later, {}, {}).status == Status::Applied);
  check(recovered.state().learning_command_receipts.size() == 1);
  check(recovered.state().learning_command_expired_through_ms == pause.expires_at_ms);
  check(recovery.execute(pause, binding, ctx, now, {}, {}).status == Status::Stale);

  for (std::size_t i = 1; i < domain::LearningCommandReceipt::kCapacity; ++i) {
    auto change = request(i % 2 ? Kind::ResumeTask : Kind::PauseTask);
    change.expires_at_ms = later + 30'000;
    check(recovery.execute(change, binding, ctx, later, {}, {}).status == Status::Applied);
  }
  auto overflow = request(Kind::PauseTask);
  overflow.expires_at_ms = later + 30'000;
  check(recovery.execute(overflow, binding, ctx, later, {}, {}).status == Status::Busy);
  check(recovery.execute(new_pause, binding, ctx, later, {}, {}).status == Status::Applied);

  // A signed result is produced only after the actual handler has committed.
  sync::FakeOutboxStorage signed_storage(std::make_shared<sync::FakeDisk>());
  AppCoordinator signed_coordinator(signed_storage, reducer);
  auto signed_task = task;
  signed_task.child_id = {"child-test"};
  check(signed_coordinator.applyTodaySnapshot({signed_task}));
  LearningVoiceCommands signed_commands(signed_coordinator, reminders);
  LearningVoiceBinding paired{{"dev-test"}, {"child-test"}, 42};
  auto paired_ctx = ctx;
  paired_ctx.device_id = paired.device_id;
  paired_ctx.child_id = paired.child_id;
  const LearningVoiceEnvelope envelope{"test-command", "START_TASK", 42, now + 30'000};
  const std::string request_mac = "bSRk9yLJn86DkrYFg7MfntB+Ljcger4362hEMfP18Bw=";
  const std::string result_mac = "5fCWQLY1rW6vym+iUCKYoZlxR+OD2PnN6KHSQVV+lkI=";
  const std::string fields = "dev-test\nchild-test\n42\ntest-command\nSTART_TASK\n1756718560000";
  auto sign = [&](std::string_view message) -> std::string {
    if (message == "claw4-learning-request-v1\n" + fields) return request_mac;
    if (message == "claw4-learning-result-v1\n" + fields + "\nAPPLIED\n-\n-") return result_mac;
    return {};
  };
  auto exchange = [&](const auto& input, std::string_view mac,
                      std::optional<LearningVoiceBinding> current, int64_t when) {
    return exchangeLearningVoice(signed_commands, input, mac, current, sign, paired_ctx,
                                 when, {}, {});
  };
  check(!exchange(envelope, request_mac, {}, now));
  check(!exchange(envelope, result_mac, paired, now));
  check(!exchange(envelope, request_mac, paired, envelope.expires_at_ms));
  auto wrong_kind = envelope;
  wrong_kind.kind = "PAUSE_TASK";
  check(!exchange(wrong_kind, request_mac, paired, now));
  auto wrong_session = paired;
  ++wrong_session.session_epoch;
  check(!exchange(envelope, request_mac, wrong_session, now));
  check(signed_coordinator.pendingCount() == 0);
  const auto signed_result = exchange(envelope, request_mac, paired, now);
  check(signed_result && signed_result->status == "APPLIED" && signed_result->mac == result_mac);
  check(signed_coordinator.pendingCount() == 2);
  check(exchange(envelope, request_mac, paired, now).has_value());
  check(signed_coordinator.pendingCount() == 2);
}
