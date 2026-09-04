// claw4/firmware/tests/unit/metalio/outbox_codec_tests.cpp
// WB-LEARNING-V4-L1 — OutboxState codec roundtrip + corruption negative tests.
// Host-only (pure C++17); the same blob layout persists on-device via NVS.
#include <cstdio>
#include <map>
#include <string>

#include "learning_domain/domain_state.h"
#include "metalio_claw4/device/core/outbox_codec.h"
#include "metalio_claw4/device/core/random_id.h"
#include "sync/outbox_storage.h"

namespace {

using claw4::domain::ChildId;
using claw4::domain::CompletionType;
using claw4::domain::DeviceId;
using claw4::domain::DeviceState;
using claw4::domain::EventId;
using claw4::domain::EventType;
using claw4::domain::SessionId;
using claw4::domain::SessionStatus;
using claw4::domain::StudySession;
using claw4::domain::Task;
using claw4::domain::TaskId;
using claw4::domain::TaskStatus;
using claw4::domain::TimestampSource;
using claw4::metalio::decodeOutboxState;
using claw4::metalio::encodeOutboxState;
using claw4::metalio::formatEntropyId;
using claw4::sync::OutboxState;
using claw4::sync::PendingEvent;

int g_fail = 0;
#define CHECK(x)                                  \
  do {                                            \
    if (!(x)) {                                   \
      std::printf("FAIL %s:%d: %s\n", __FILE__,  \
                  __LINE__, #x);                  \
      ++g_fail;                                   \
    }                                             \
  } while (0)

Task MakeTask(const char* id, const char* title, const char* subject, int minutes,
              TaskStatus status, int64_t version, const char* date) {
  Task t;
  t.task_id = TaskId{id};
  t.child_id = ChildId{"child-1"};
  t.title = title;
  t.subject = subject;
  t.description = "desc line1\nline2 with tab\tinside";
  t.task_type = "practice";
  t.estimated_minutes = minutes;
  t.priority = "high";
  t.status = status;
  t.scheduled_date = date;
  t.version = version;
  return t;
}

PendingEvent MakeEvent(const char* id, int64_t seq, EventType type,
                       const char* k1, const char* v1, const char* k2,
                       const char* v2, bool deadletter = false) {
  PendingEvent p;
  p.event_id = EventId{id};
  p.device_id = DeviceId{"dev-metalio-1"};
  p.child_id = ChildId{"child-1"};
  p.sequence = seq;
  p.timestamp = 1700000000 + seq;
  p.timestamp_source = TimestampSource::Local;
  p.type = type;
  p.version = 1;
  if (k1) p.payload[k1] = v1;
  if (k2) p.payload[k2] = v2;
  if (deadletter) p.dead_letter_reason = std::string("business_4xx_http=409");
  return p;
}

// Full-featured state: unicode/escape-heavy strings + session + pending rows.
OutboxState MakeFullState() {
  OutboxState st;
  st.domain.device_state = DeviceState::Focusing;
  st.domain.tasks = {
      MakeTask("task-口算", "口算练习", "math", 20, TaskStatus::InProgress, 7,
               "2026-09-03"),
      MakeTask("task-2", "背诵 唐诗\\宋词\t（含转义）", "chinese", 15,
               TaskStatus::Ready, 3, "2026-09-03"),
  };
  StudySession sess;
  sess.session_id = SessionId{"sess-001"};
  sess.task_id = TaskId{"task-口算"};
  sess.child_id = ChildId{"child-1"};
  sess.device_id = DeviceId{"dev-metalio-1"};
  sess.planned_minutes = 20;
  sess.actual_seconds = 540;
  sess.pause_count = 2;
  sess.pause_seconds = 60;
  sess.status = SessionStatus::Running;
  sess.segment_start_monotonic_ms = 123456789;
  st.domain.active_session = sess;
  st.pending = {
      MakeEvent("ev-1", 1, EventType::TaskStarted, "task_id", "task-口算",
                "title", "口算练习"),
      MakeEvent("ev-2", 2, EventType::StudySessionStarted, "session_id",
                "sess-001", "planned_minutes", "20"),
      MakeEvent("ev-3", 3, EventType::TaskCompleted, "task_id", "task-口算",
                "completed_at", "1700001000", /*deadletter=*/true),
  };
  st.next_sequence = 4;
  st.last_acked_sequence = 2;
  st.diagnostic.sync_failed = true;
  st.diagnostic.sync_recovered = false;
  return st;
}

bool SameTask(const Task& a, const Task& b) {
  return a.task_id.value == b.task_id.value &&
         a.child_id.value == b.child_id.value && a.title == b.title &&
         a.subject == b.subject && a.description == b.description &&
         a.task_type == b.task_type &&
         a.estimated_minutes == b.estimated_minutes && a.priority == b.priority &&
         a.status == b.status && a.scheduled_date == b.scheduled_date &&
         a.version == b.version;
}

bool SamePending(const PendingEvent& a, const PendingEvent& b) {
  return a.event_id.value == b.event_id.value &&
         a.device_id.value == b.device_id.value &&
         a.child_id.value == b.child_id.value && a.sequence == b.sequence &&
         a.timestamp == b.timestamp && a.timestamp_source == b.timestamp_source &&
         a.type == b.type && a.version == b.version && a.payload == b.payload &&
         a.dead_letter_reason == b.dead_letter_reason;
}

bool RunFullRoundtrip() {
  const OutboxState st = MakeFullState();
  std::string blob;
  CHECK(encodeOutboxState(st, blob));
  CHECK(!blob.empty());
  OutboxState out;
  CHECK(decodeOutboxState(blob, out));
  CHECK(out.domain.device_state == st.domain.device_state);
  CHECK(out.domain.tasks.size() == st.domain.tasks.size());
  for (size_t i = 0; i < st.domain.tasks.size(); ++i) {
    CHECK(SameTask(out.domain.tasks[i], st.domain.tasks[i]));
  }
  CHECK(out.domain.active_session.has_value());
  CHECK(out.domain.active_session->session_id.value == "sess-001");
  CHECK(out.domain.active_session->actual_seconds == 540);
  CHECK(out.domain.active_session->segment_start_monotonic_ms == 123456789);
  CHECK(out.pending.size() == 3);
  for (size_t i = 0; i < st.pending.size(); ++i) {
    CHECK(SamePending(out.pending[i], st.pending[i]));
  }
  CHECK(out.pending[2].dead_letter_reason.has_value());
  CHECK(out.pending[2].dead_letter_reason.value() == "business_4xx_http=409");
  CHECK(out.next_sequence == 4);
  CHECK(out.last_acked_sequence == 2);
  CHECK(out.diagnostic.sync_failed == true);
  CHECK(out.diagnostic.sync_recovered == false);
  return g_fail == 0;
}

bool RunEmptyRoundtrip() {
  OutboxState st;  // all defaults
  std::string blob;
  CHECK(encodeOutboxState(st, blob));
  OutboxState out;
  CHECK(decodeOutboxState(blob, out));
  CHECK(out.domain.tasks.empty());
  CHECK(!out.domain.active_session.has_value());
  CHECK(out.pending.empty());
  CHECK(out.next_sequence == 1);
  CHECK(out.last_acked_sequence == 0);
  return true;
}

bool RunIdempotentRoundtrip() {
  // encode(decode(encode(x))) must be stable (bit-identical) — guarantees NVS
  // rewrites are no-ops when nothing changed.
  const OutboxState st = MakeFullState();
  std::string b1, b2;
  CHECK(encodeOutboxState(st, b1));
  OutboxState mid;
  CHECK(decodeOutboxState(b1, mid));
  CHECK(encodeOutboxState(mid, b2));
  CHECK(b1 == b2);
  return true;
}

bool RunCorruptionNegatives() {
  OutboxState st = MakeFullState();
  std::string blob;
  CHECK(encodeOutboxState(st, blob));
  OutboxState out;
  // wrong magic
  CHECK(!decodeOutboxState("NOPE" + blob.substr(4), out));
  // truncated (cut in the middle of the task section)
  CHECK(!decodeOutboxState(blob.substr(0, blob.size() / 2), out));
  // structural corruption: break a record tag -> unknown record -> decode fail
  {
    std::string bad = blob;
    const size_t tag = bad.find("\nt|");  // first task row
    CHECK(tag != std::string::npos);
    bad[tag + 1] = 'Z';
    CHECK(!decodeOutboxState(bad, out));
  }
  // structural corruption: bump task count 2 -> 9 (rows underflow at EOF)
  {
    std::string bad = blob;
    const size_t t = bad.find("T|2\n");
    CHECK(t != std::string::npos);
    bad[t + 2] = '9';
    CHECK(!decodeOutboxState(bad, out));
  }
  // trailing junk record appended at end
  CHECK(!decodeOutboxState(blob + "JUNK\n", out));
  // empty / tiny
  CHECK(!decodeOutboxState("", out));
  CHECK(!decodeOutboxState("C4L1OUTBOX", out));
  return true;
}

bool RunEntropyIdFormatting() {
  CHECK(formatEntropyId("ev-", 0x01234567U, 0x89abcdefU) ==
        "ev-0123456789abcdef");
  CHECK(formatEntropyId("ev-", 0U, 1U) == "ev-0000000000000001");
  CHECK(formatEntropyId("sess-", 0xffffffffU, 0U) ==
        "sess-ffffffff00000000");
  CHECK(formatEntropyId("ev-", 0x01234567U, 0x89abcdefU) !=
        formatEntropyId("ev-", 0x01234567U, 0x89abcdeeU));
  return true;
}

}  // namespace

int main() {
  RunFullRoundtrip();
  RunEmptyRoundtrip();
  RunIdempotentRoundtrip();
  RunCorruptionNegatives();
  RunEntropyIdFormatting();
  if (g_fail == 0) {
    std::printf("outbox_codec_tests: all PASS\n");
    return 0;
  }
  std::printf("outbox_codec_tests: %d FAILURES\n", g_fail);
  return 1;
}
