// claw4/integration/metalio_claw4/device/core/outbox_codec.cpp
// WB-LEARNING-V4-L1 — OutboxState <-> string codec (see header for format).
#include "metalio_claw4/device/core/outbox_codec.h"

#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>

namespace claw4 {
namespace metalio {
namespace {

constexpr char kSep = '\t';
constexpr char kMapSep = '\x1e';  // between payload k=v pairs
constexpr char kPairSep = '\x1f'; // between payload key and value

std::string Escape(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  for (char c : in) {
    if (c == '\\') out += "\\\\";
    else if (c == '\t') out += "\\t";
    else if (c == '\n') out += "\\n";
    else out += c;
  }
  return out;
}

std::string Unescape(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '\\' && i + 1 < in.size()) {
      char n = in[i + 1];
      if (n == '\\') { out += '\\'; ++i; }
      else if (n == 't') { out += '\t'; ++i; }
      else if (n == 'n') { out += '\n'; ++i; }
      else out += in[i];
    } else {
      out += in[i];
    }
  }
  return out;
}

std::string PayloadToStr(const std::map<std::string, std::string>& payload) {
  std::string out;
  bool first = true;
  for (const auto& kv : payload) {
    if (!first) out += kMapSep;
    first = false;
    out += Escape(kv.first) + kPairSep + Escape(kv.second);
  }
  return out;
}

bool StrToPayload(const std::string& in, std::map<std::string, std::string>& payload) {
  payload.clear();
  size_t i = 0;
  while (i < in.size()) {
    size_t sep = in.find(kMapSep, i);
    std::string pair = (sep == std::string::npos) ? in.substr(i) : in.substr(i, sep - i);
    size_t kv = pair.find(kPairSep);
    if (kv == std::string::npos) return false;  // malformed pair
    payload[Unescape(pair.substr(0, kv))] = Unescape(pair.substr(kv + 1));
    if (sep == std::string::npos) break;
    i = sep + 1;
  }
  return true;
}

template <typename T>
std::string I(T v) {
  return std::to_string(static_cast<long long>(v));
}

// --- row helpers ---------------------------------------------------------
std::string TaskRow(const claw4::domain::Task& t) {
  std::ostringstream o;
  o << "t|" << t.task_id.value << kSep << t.child_id.value << kSep << Escape(t.title)
    << kSep << Escape(t.subject) << kSep << Escape(t.description) << kSep
    << Escape(t.task_type) << kSep << I(t.estimated_minutes) << kSep
    << Escape(t.priority) << kSep << I(static_cast<int>(t.status)) << kSep
    << Escape(t.scheduled_date) << kSep << I(t.version);
  return o.str();
}

bool ParseTaskRow(const std::vector<std::string>& f, claw4::domain::Task& t) {
  if (f.size() != 11) return false;
  t.task_id = claw4::domain::TaskId{f[0]};
  t.child_id = claw4::domain::ChildId{f[1]};
  t.title = Unescape(f[2]);
  t.subject = Unescape(f[3]);
  t.description = Unescape(f[4]);
  t.task_type = Unescape(f[5]);
  t.estimated_minutes = std::atoi(f[6].c_str());
  t.priority = Unescape(f[7]);
  t.status = static_cast<claw4::domain::TaskStatus>(std::atoi(f[8].c_str()));
  t.scheduled_date = Unescape(f[9]);
  t.version = std::atoll(f[10].c_str());
  return true;
}

std::string SessionRow(const claw4::domain::StudySession& s) {
  std::ostringstream o;
  o << "s|" << s.session_id.value << kSep << s.task_id.value << kSep << s.child_id.value
    << kSep << s.device_id.value << kSep << I(s.planned_minutes) << kSep
    << I(s.actual_seconds) << kSep << I(s.pause_count) << kSep
    << I(s.pause_seconds) << kSep << I(static_cast<int>(s.status)) << kSep;
  if (s.completion_type.has_value()) {
    o << I(static_cast<int>(*s.completion_type));
  }
  o << kSep << I(s.segment_start_monotonic_ms) << kSep
    << I(s.paused_at_monotonic_ms);
  return o.str();
}

bool ParseSessionRow(const std::vector<std::string>& f,
                     claw4::domain::StudySession& s) {
  if (f.size() != 12) return false;
  s.session_id = claw4::domain::SessionId{f[0]};
  s.task_id = claw4::domain::TaskId{f[1]};
  s.child_id = claw4::domain::ChildId{f[2]};
  s.device_id = claw4::domain::DeviceId{f[3]};
  s.planned_minutes = std::atoi(f[4].c_str());
  s.actual_seconds = std::atoll(f[5].c_str());
  s.pause_count = std::atoi(f[6].c_str());
  s.pause_seconds = std::atoll(f[7].c_str());
  s.status = static_cast<claw4::domain::SessionStatus>(std::atoi(f[8].c_str()));
  if (f[9].empty()) {
    s.completion_type.reset();
  } else {
    s.completion_type =
        static_cast<claw4::domain::CompletionType>(std::atoi(f[9].c_str()));
  }
  s.segment_start_monotonic_ms = std::atoll(f[10].c_str());
  s.paused_at_monotonic_ms = std::atoll(f[11].c_str());
  return true;
}

std::string PendingRow(const claw4::sync::PendingEvent& p) {
  std::ostringstream o;
  o << "p|" << p.event_id.value << kSep << p.device_id.value << kSep << p.child_id.value
    << kSep << I(p.sequence) << kSep << I(p.timestamp) << kSep
    << I(static_cast<int>(p.timestamp_source)) << kSep
    << I(static_cast<int>(p.type)) << kSep << I(p.version) << kSep
    << PayloadToStr(p.payload) << kSep
    << (p.dead_letter_reason.has_value() ? Escape(*p.dead_letter_reason)
                                         : std::string());
  return o.str();
}

bool ParsePendingRow(const std::vector<std::string>& f,
                     claw4::sync::PendingEvent& p) {
  if (f.size() != 10) return false;
  p.event_id = claw4::domain::EventId{f[0]};
  p.device_id = claw4::domain::DeviceId{f[1]};
  p.child_id = claw4::domain::ChildId{f[2]};
  p.sequence = std::atoll(f[3].c_str());
  p.timestamp = std::atoll(f[4].c_str());
  p.timestamp_source =
      static_cast<claw4::domain::TimestampSource>(std::atoi(f[5].c_str()));
  p.type = static_cast<claw4::domain::EventType>(std::atoi(f[6].c_str()));
  p.version = std::atoi(f[7].c_str());
  if (!StrToPayload(f[8], p.payload)) return false;
  p.dead_letter_reason =
      f[9].empty() ? std::optional<std::string>{}
                   : std::optional<std::string>{Unescape(f[9])};
  return true;
}

std::vector<std::string> Split(const std::string& line) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i <= line.size()) {
    size_t sep = line.find(kSep, i);
    if (sep == std::string::npos) {
      out.push_back(line.substr(i));
      break;
    }
    out.push_back(line.substr(i, sep - i));
    i = sep + 1;
  }
  return out;
}

bool ReadInt(const std::string& s, int64_t& v) {
  size_t used = 0;
  try {
    v = std::stoll(s, &used);
  } catch (...) {
    return false;
  }
  return used == s.size();
}

}  // namespace

bool encodeOutboxState(const claw4::sync::OutboxState& st, std::string& out) {
  std::ostringstream o;
  o << kOutboxCodecMagic << '\n';
  o << "D|" << I(static_cast<int>(st.domain.device_state)) << '\n';
  o << "T|" << I(st.domain.tasks.size()) << '\n';
  for (const auto& t : st.domain.tasks) o << TaskRow(t) << '\n';
  o << "S|" << (st.domain.active_session.has_value() ? "1" : "0") << '\n';
  if (st.domain.active_session.has_value()) {
    o << SessionRow(*st.domain.active_session) << '\n';
  }
  o << "E|" << I(st.pending.size()) << '\n';
  for (const auto& p : st.pending) o << PendingRow(p) << '\n';
  o << "Q|" << I(st.next_sequence) << '\n';
  o << "A|" << I(st.last_acked_sequence) << '\n';
  o << "F|" << (st.diagnostic.sync_failed ? "1" : "0") << '\n';
  o << "R|" << (st.diagnostic.sync_recovered ? "1" : "0") << '\n';
  out = o.str();
  if (out.size() > kOutboxCodecMaxBytes) {
    out.clear();
    return false;
  }
  return true;
}

bool decodeOutboxState(const std::string& in, claw4::sync::OutboxState& out) {
  if (in.size() > kOutboxCodecMaxBytes || in.size() < 16) return false;
  claw4::sync::OutboxState st;
  std::istringstream is(in);
  std::string line;
  if (!std::getline(is, line) || line != kOutboxCodecMagic) return false;
  int64_t tasks_left = -1, sessions_flag = -1, pending_left = -1;
  bool have_next = false, have_acked = false, have_f = false, have_r = false;
  while (std::getline(is, line)) {
    if (line.empty()) continue;
    const char tag = line[0];
    std::vector<std::string> f = Split(line.substr(2));
    switch (tag) {
      case 'D': {
        int64_t v;
        if (!ReadInt(f[0], v)) return false;
        st.domain.device_state = static_cast<claw4::domain::DeviceState>(v);
        break;
      }
      case 'T': {
        int64_t v;
        if (!ReadInt(f[0], v) || v < 0 || v > 256) return false;
        tasks_left = v;
        break;
      }
      case 't': {
        if (tasks_left <= 0) return false;
        claw4::domain::Task t;
        if (!ParseTaskRow(f, t)) return false;
        st.domain.tasks.push_back(std::move(t));
        --tasks_left;
        break;
      }
      case 'S': {
        sessions_flag = f[0] == "1" ? 1 : 0;
        break;
      }
      case 's': {
        if (sessions_flag != 1 || st.domain.active_session.has_value()) {
          return false;
        }
        claw4::domain::StudySession s;
        if (!ParseSessionRow(f, s)) return false;
        st.domain.active_session = std::move(s);
        sessions_flag = 0;  // consumed
        break;
      }
      case 'E': {
        int64_t v;
        if (!ReadInt(f[0], v) || v < 0 || v > 200) return false;
        pending_left = v;
        break;
      }
      case 'p': {
        if (pending_left <= 0) return false;
        claw4::sync::PendingEvent p;
        if (!ParsePendingRow(f, p)) return false;
        st.pending.push_back(std::move(p));
        --pending_left;
        break;
      }
      case 'Q': {
        if (!ReadInt(f[0], st.next_sequence)) return false;
        have_next = true;
        break;
      }
      case 'A': {
        if (!ReadInt(f[0], st.last_acked_sequence)) return false;
        have_acked = true;
        break;
      }
      case 'F': {
        if (f[0] != "0" && f[0] != "1") return false;
        st.diagnostic.sync_failed = f[0] == "1";
        have_f = true;
        break;
      }
      case 'R': {
        if (f[0] != "0" && f[0] != "1") return false;
        st.diagnostic.sync_recovered = f[0] == "1";
        have_r = true;
        break;
      }
      default:
        return false;  // unknown record -> corrupt
    }
  }
  if (tasks_left != 0 || sessions_flag != 0 || pending_left != 0 ||
      !have_next || !have_acked || !have_f || !have_r) {
    return false;
  }
  out = std::move(st);
  return true;
}

}  // namespace metalio
}  // namespace claw4
