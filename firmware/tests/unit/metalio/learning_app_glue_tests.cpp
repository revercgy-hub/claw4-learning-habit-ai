// claw4/firmware/tests/unit/metalio/learning_app_glue_tests.cpp
// P17a acceptance: production LearningApp glue over the REAL coordinator is
// driven end-to-end exactly like the device shell will (WB-LEARNING-V4-NEXT
// 阶段 E). Uses deterministic fakes for storage/clock only.
//
// Compiles/links/runs natively on Windows; exit 0 on pass.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "fakes/fake_outbox_storage.h"
#include "fakes/fake_platform_ports.h"
#include "metalio_claw4/device/core/demo_seed.h"
#include "metalio_claw4/device/core/outbox_codec.h"
#include "metalio_claw4/host_glue/learning_app.h"

using namespace claw4::domain;
using namespace claw4::sync;
using namespace claw4::ports;
using namespace claw4::metalio;
using namespace claw4::interaction;
using namespace claw4::mcp;

static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                                   \
  do {                                               \
    ++g_cases;                                       \
    if (!run_case_##name()) {                        \
      std::printf("FAIL: %s\n", #name);              \
      ++g_fail;                                      \
    }                                                \
  } while (0)

#define CHECK(cond)                                  \
  do {                                               \
    if (!(cond)) {                                   \
      std::printf("  assert fail: %s (line %d)\n",   \
                  #cond, __LINE__);                  \
      return false;                                  \
    }                                                \
  } while (0)

namespace {

struct CodecDisk {
  std::string blob;
};

// Host model of NvsOutboxStorage: every load decodes the persisted blob and
// every mutation encodes a complete replacement before publishing it. This
// exercises the same restart boundary and codec path as the device adapter.
class CodecOutboxStorage final : public OutboxStorage {
 public:
  explicit CodecOutboxStorage(std::shared_ptr<CodecDisk> disk)
      : disk_(std::move(disk)) {}

  bool load(OutboxState& out) override {
    if (disk_->blob.empty()) {
      out = OutboxState{};
      return true;
    }
    return decodeOutboxState(disk_->blob, out);
  }

  CommitStatus commit(const DomainState& next_domain,
                      const std::vector<PendingEvent>& appended,
                      int64_t next_sequence) override {
    OutboxState cur;
    if (!load(cur)) return CommitStatus::StorageError;
    cur.domain = next_domain;
    cur.pending.insert(cur.pending.end(), appended.begin(), appended.end());
    cur.next_sequence = next_sequence;
    return save(cur);
  }

  CommitStatus commitDiagnostic(bool sync_failed,
                                bool sync_recovered) override {
    OutboxState cur;
    if (!load(cur)) return CommitStatus::StorageError;
    cur.diagnostic.sync_failed = sync_failed;
    cur.diagnostic.sync_recovered = sync_recovered;
    return save(cur);
  }

  CommitStatus removeAcked(int64_t up_to_sequence) override {
    OutboxState cur;
    if (!load(cur)) return CommitStatus::StorageError;
    cur.pending.erase(
        std::remove_if(cur.pending.begin(), cur.pending.end(),
                       [&](const PendingEvent& row) {
                         return row.sequence <= up_to_sequence;
                       }),
        cur.pending.end());
    cur.last_acked_sequence =
        std::max(cur.last_acked_sequence, up_to_sequence);
    return save(cur);
  }

  CommitStatus markDeadLetter(const EventId& event_id,
                              const std::string& reason) override {
    OutboxState cur;
    if (!load(cur)) return CommitStatus::StorageError;
    for (auto& row : cur.pending) {
      if (row.event_id == event_id) {
        row.dead_letter_reason = reason;
        break;
      }
    }
    return save(cur);
  }

 private:
  CommitStatus save(const OutboxState& state) {
    std::string next_blob;
    if (!encodeOutboxState(state, next_blob)) {
      return CommitStatus::StorageError;
    }
    disk_->blob = std::move(next_blob);
    return CommitStatus::Committed;
  }

  std::shared_ptr<CodecDisk> disk_;
};

struct Env {
  std::shared_ptr<FakeDisk> disk = std::make_shared<FakeDisk>();
  FakeOutboxStorage storage{disk};
  FakeClockPort clock;
  LearningApp app{storage, clock};

  Env() {
    clock.epoch = 1756718530;
    clock.mono = 1'000'000;
    clock.synced = false;  // device may boot without SNTP; domain handles it
    app.start();
  }

  void addTask(const char* id, const char* title, int minutes, TaskStatus status) {
    Task t;
    t.task_id = TaskId{id};
    t.child_id = ChildId{"child-1"};
    t.title = title;
    t.subject = "math";
    t.estimated_minutes = minutes;
    t.status = status;
    t.version = 1;
    today_.push_back(t);
    app.applyTodaySnapshot(today_);
  }
  std::vector<Task> today_;
};

McpRequest mcp(const char* tool, const char* task_id = nullptr) {
  McpRequest r;
  r.tool = tool;
  if (task_id) r.args["task_id"] = task_id;
  return r;
}

}  // namespace

static bool run_case_app_lifecycle_and_start_task() {
  Env e;
  CHECK(e.app.running());
  e.addTask("t1", "数学口算", 20, TaskStatus::Ready);

  const auto resp = e.app.mcpHost().invoke(mcp(LearningMcpHost::kToolStartTask, "t1"));
  CHECK(resp.ok);
  CHECK(resp.payload.find("\"intent_result\":\"accepted\"") != std::string::npos);
  CHECK(e.app.state().tasks[0].status == TaskStatus::InProgress);
  CHECK(e.app.state().active_session.has_value());
  CHECK(e.app.pendingCount() == 2);

  e.app.stop();
  CHECK(!e.app.running());
  return true;
}

static bool run_case_app_authoritative_snapshot_and_reboot() {
  Env e;
  e.addTask("t1", "A", 10, TaskStatus::Ready);
  e.addTask("t2", "B", 10, TaskStatus::Ready);
  CHECK(e.app.state().tasks.size() == 2);
  // Authoritative: server now only lists t1 -> t2 removed.
  e.today_.erase(e.today_.begin() + 1);
  e.app.applyTodaySnapshot(e.today_);
  CHECK(e.app.state().tasks.size() == 1);
  CHECK(e.app.state().tasks[0].task_id == TaskId{"t1"});

  // "Reboot": a brand-new app over the same disk sees the same snapshot.
  FakeClockPort clock2;
  clock2.mono = 2'000'000;
  LearningApp app2{e.storage, clock2};
  CHECK(app2.state().tasks.size() == 1);
  CHECK(app2.state().tasks[0].task_id == TaskId{"t1"});
  return true;
}

static bool run_case_app_codec_reboot_old_pending_then_pause() {
  auto disk = std::make_shared<CodecDisk>();
  CodecOutboxStorage storage{disk};
  int event_seq = 0;
  int session_seq = 0;

  FakeClockPort first_clock;
  first_clock.epoch = 1756718530;
  first_clock.mono = 1'000'000;
  {
    LearningApp first{storage, first_clock};
    first.setIdentity(DeviceId{"dev-claw4-l1"}, ChildId{"child-1"});
    first.setEventIdFactory([&]() {
      return EventId{"ev-" + std::to_string(++event_seq)};
    });
    first.setSessionIdFactory([&]() {
      return SessionId{"sess-" + std::to_string(++session_seq)};
    });
    CHECK(first.applyTodaySnapshot(DemoTodaySnapshot()));
    CommandPayload start;
    start.kind = CommandKind::StartTask;
    start.task_id = TaskId{"demo-math-001"};
    const auto result = first.dispatcher().dispatch(CommandSource::Touch, start);
    CHECK(result.intent_result == IntentResult::Accepted);
    CHECK(first.pendingCount() == 2);
  }

  OutboxState after_start;
  CHECK(storage.load(after_start));
  CHECK(after_start.pending.size() == 2);
  CHECK(after_start.pending[0].event_id != after_start.pending[1].event_id);
  CHECK(after_start.next_sequence == 3);

  // Reboot: reconstruct the complete app over the encoded persisted blob,
  // retain the old pending rows, then issue the next real state transition.
  FakeClockPort second_clock;
  second_clock.epoch = 1756718535;
  second_clock.mono = 1'005'000;
  LearningApp second{storage, second_clock};
  second.setIdentity(DeviceId{"dev-claw4-l1"}, ChildId{"child-1"});
  second.setEventIdFactory([&]() {
    return EventId{"ev-" + std::to_string(++event_seq)};
  });
  second.setSessionIdFactory([&]() {
    return SessionId{"sess-" + std::to_string(++session_seq)};
  });
  CommandPayload pause;
  pause.kind = CommandKind::PauseTask;
  pause.task_id = TaskId{"demo-math-001"};
  const auto result = second.dispatcher().dispatch(CommandSource::Touch, pause);
  CHECK(result.intent_result == IntentResult::Accepted);
  CHECK(second.state().tasks[0].status == TaskStatus::Paused);
  CHECK(second.state().active_session.has_value());
  CHECK(second.state().active_session->status == SessionStatus::Paused);
  CHECK(second.pendingCount() == 3);

  OutboxState after_pause;
  CHECK(storage.load(after_pause));
  CHECK(after_pause.pending.size() == 3);
  CHECK(after_pause.pending[2].event_id == EventId{"ev-3"});
  CHECK(after_pause.next_sequence == 4);
  return true;
}

static bool run_case_app_mcp_complete_gate_via_confirm() {
  Env e;
  e.addTask("t1", "数学口算", 20, TaskStatus::Ready);
  CHECK(e.app.mcpHost().invoke(mcp(LearningMcpHost::kToolStartTask, "t1")).ok);

  const auto req = e.app.mcpHost().invoke(
      mcp(LearningMcpHost::kToolRequestCompleteTask, "t1"));
  CHECK(req.ok);
  CHECK(req.payload.find("\"confirmation\":\"pending\"") != std::string::npos);
  // AI path leaves the domain untouched; physical confirm completes it.
  CHECK(e.app.state().tasks[0].status == TaskStatus::InProgress);
  const auto cr = e.app.dispatcher().confirmPendingComplete();
  CHECK(cr.status == DispatchStatus::Emitted);
  CHECK(e.app.state().tasks[0].status == TaskStatus::Completed);
  CHECK(!e.app.state().active_session.has_value());
  return true;
}

static bool run_case_app_auth_pause_passthrough() {
  Env e;
  e.addTask("t1", "A", 10, TaskStatus::Ready);
  CHECK(e.app.mcpHost().invoke(mcp(LearningMcpHost::kToolStartTask, "t1")).ok);
  CHECK(e.app.pendingCount() == 2);
  e.app.resetAuthPause();  // idle until a real sync runs
  CHECK(!e.app.authPaused());
  return true;
}

static bool run_case_all() {
  CASE(app_lifecycle_and_start_task);
  CASE(app_authoritative_snapshot_and_reboot);
  CASE(app_codec_reboot_old_pending_then_pause);
  CASE(app_mcp_complete_gate_via_confirm);
  CASE(app_auth_pause_passthrough);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-LEARNING-V4-NEXT P17a LearningApp glue tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
