// CODEX-APP-FIRST-001 AF1 — portable JSON wire codec implementation.
#include "sync/wire_codec.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

namespace claw4 {
namespace sync {
namespace wire {

namespace {

struct JsonValue {
  enum class Kind { Null, Number, String, Object, Array };
  Kind kind = Kind::Null;
  int64_t number = 0;
  std::string string;
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
};

class JsonParser {
 public:
  explicit JsonParser(const std::string& text) : text_(text) {}

  bool Parse(JsonValue& value) {
    SkipWhitespace();
    if (!ParseValue(value)) return false;
    SkipWhitespace();
    return pos_ == text_.size();
  }

 private:
  void SkipWhitespace() {
    while (pos_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
  }

  bool Consume(const char expected) {
    if (pos_ >= text_.size() || text_[pos_] != expected) return false;
    ++pos_;
    return true;
  }

  bool ParseValue(JsonValue& value) {
    SkipWhitespace();
    if (pos_ >= text_.size()) return false;
    switch (text_[pos_]) {
      case '{':
        return ParseObject(value);
      case '[':
        return ParseArray(value);
      case '"':
        value.kind = JsonValue::Kind::String;
        return ParseString(value.string);
      case 'n':
        if (text_.compare(pos_, 4, "null") != 0) return false;
        pos_ += 4;
        value.kind = JsonValue::Kind::Null;
        return true;
      default:
        return ParseNumber(value);
    }
  }

  bool ParseObject(JsonValue& value) {
    if (!Consume('{')) return false;
    value.kind = JsonValue::Kind::Object;
    value.object.clear();
    SkipWhitespace();
    if (Consume('}')) return true;
    while (pos_ < text_.size()) {
      std::string key;
      if (!ParseString(key)) return false;
      SkipWhitespace();
      if (!Consume(':')) return false;
      JsonValue child;
      if (!ParseValue(child)) return false;
      value.object[std::move(key)] = std::move(child);
      SkipWhitespace();
      if (Consume('}')) return true;
      if (!Consume(',')) return false;
      SkipWhitespace();
    }
    return false;
  }

  bool ParseArray(JsonValue& value) {
    if (!Consume('[')) return false;
    value.kind = JsonValue::Kind::Array;
    value.array.clear();
    SkipWhitespace();
    if (Consume(']')) return true;
    while (pos_ < text_.size()) {
      JsonValue child;
      if (!ParseValue(child)) return false;
      value.array.push_back(std::move(child));
      SkipWhitespace();
      if (Consume(']')) return true;
      if (!Consume(',')) return false;
      SkipWhitespace();
    }
    return false;
  }

  bool ParseString(std::string& out) {
    if (!Consume('"')) return false;
    out.clear();
    while (pos_ < text_.size()) {
      const char ch = text_[pos_++];
      if (ch == '"') return true;
      if (ch != '\\') {
        out.push_back(ch);
        continue;
      }
      if (pos_ >= text_.size()) return false;
      const char escaped = text_[pos_++];
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          out.push_back(escaped);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u':
          if (!ParseUnicodeEscape(out)) return false;
          break;
        default:
          return false;
      }
    }
    return false;
  }

  bool ParseUnicodeEscape(std::string& out) {
    if (pos_ + 4 > text_.size()) return false;
    uint32_t codepoint = 0;
    for (int i = 0; i < 4; ++i) {
      const char ch = text_[pos_++];
      codepoint <<= 4;
      if (ch >= '0' && ch <= '9') codepoint += static_cast<uint32_t>(ch - '0');
      else if (ch >= 'a' && ch <= 'f') codepoint += static_cast<uint32_t>(ch - 'a' + 10);
      else if (ch >= 'A' && ch <= 'F') codepoint += static_cast<uint32_t>(ch - 'A' + 10);
      else return false;
    }
    if (codepoint <= 0x7f) {
      out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
      out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
      out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    return true;
  }

  bool ParseNumber(JsonValue& value) {
    const std::size_t start = pos_;
    if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
    const std::size_t digits = pos_;
    while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    if (digits == pos_) return false;
    try {
      value.number = std::stoll(text_.substr(start, pos_ - start));
    } catch (...) {
      return false;
    }
    value.kind = JsonValue::Kind::Number;
    return true;
  }

  const std::string& text_;
  std::size_t pos_ = 0;
};

std::string Quote(const std::string& value) {
  std::ostringstream out;
  out << '"';
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(ch) << std::dec;
        } else {
          out << static_cast<char>(ch);
        }
    }
  }
  out << '"';
  return out.str();
}

const JsonValue* Field(const JsonValue& object, const char* name) {
  if (object.kind != JsonValue::Kind::Object) return nullptr;
  const auto it = object.object.find(name);
  return it == object.object.end() ? nullptr : &it->second;
}

bool StringField(const JsonValue& object, const char* name, std::string& out) {
  const auto* value = Field(object, name);
  if (!value || value->kind != JsonValue::Kind::String) return false;
  out = value->string;
  return true;
}

bool NumberField(const JsonValue& object, const char* name, int64_t& out) {
  const auto* value = Field(object, name);
  if (!value || value->kind != JsonValue::Kind::Number) return false;
  out = value->number;
  return true;
}

void StartObject(std::ostringstream& out) { out << '{'; }
void EndObject(std::ostringstream& out) { out << '}'; }

void FieldPrefix(std::ostringstream& out, bool& first, const char* name) {
  if (!first) out << ',';
  first = false;
  out << Quote(name) << ':';
}

const char* EventTypeName(const claw4::domain::EventType type) {
  using claw4::domain::EventType;
  switch (type) {
    case EventType::DeviceBooted: return "device.booted";
    case EventType::DeviceOnline: return "device.online";
    case EventType::DeviceOffline: return "device.offline";
    case EventType::TaskStarted: return "task.started";
    case EventType::TaskPaused: return "task.paused";
    case EventType::TaskResumed: return "task.resumed";
    case EventType::TaskCompleted: return "task.completed";
    case EventType::TaskSkipped: return "task.skipped";
    case EventType::StudySessionStarted: return "study.session.started";
    case EventType::StudySessionCompleted: return "study.session.completed";
    case EventType::SyncFailed: return "sync.failed";
    case EventType::SyncRecovered: return "sync.recovered";
  }
  return "device.booted";
}

const char* TimestampSourceName(const claw4::domain::TimestampSource source) {
  return source == claw4::domain::TimestampSource::Rtc ? "rtc" : "local";
}

bool ParseTaskStatus(const std::string& name, claw4::domain::TaskStatus& out) {
  using claw4::domain::TaskStatus;
  if (name == "pending") out = TaskStatus::Pending;
  else if (name == "ready") out = TaskStatus::Ready;
  else if (name == "in_progress") out = TaskStatus::InProgress;
  else if (name == "paused") out = TaskStatus::Paused;
  else if (name == "completed") out = TaskStatus::Completed;
  else if (name == "skipped") out = TaskStatus::Skipped;
  else return false;
  return true;
}

bool ParseEventOutcome(const std::string& name, EventOutcome& out) {
  if (name == "accepted") out = EventOutcome::Accepted;
  else if (name == "duplicate") out = EventOutcome::Duplicate;
  else if (name == "conflict") out = EventOutcome::Conflict;
  else if (name == "rejected") out = EventOutcome::Rejected;
  else if (name == "gap") out = EventOutcome::Gap;
  else return false;
  return true;
}

bool ParseJson(const std::string& json, JsonValue& value) {
  JsonParser parser(json);
  return parser.Parse(value);
}

}  // namespace

std::string EncodeChallengeRequest(const ChallengeRequest& request) {
  std::ostringstream out;
  StartObject(out);
  out << Quote("device_id") << ':' << Quote(request.device_id);
  EndObject(out);
  return out.str();
}

bool DecodeChallengeResponse(const std::string& json,
                             ChallengeResponse& response) {
  JsonValue root;
  if (!ParseJson(json, root)) return false;
  return StringField(root, "challenge_id", response.challenge_id) &&
         StringField(root, "nonce", response.nonce) &&
         NumberField(root, "expires_at", response.expires_at);
}

std::string EncodeAuthRequest(const AuthRequest& request) {
  std::ostringstream out;
  StartObject(out);
  bool first = true;
  FieldPrefix(out, first, "device_id"); out << Quote(request.device_id);
  FieldPrefix(out, first, "challenge_id"); out << Quote(request.challenge_id);
  FieldPrefix(out, first, "nonce"); out << Quote(request.nonce);
  FieldPrefix(out, first, "challenge_signature"); out << Quote(request.challenge_signature);
  EndObject(out);
  return out.str();
}

bool DecodeAuthResponse(const std::string& json, AuthResponse& response) {
  JsonValue root;
  if (!ParseJson(json, root)) return false;
  return StringField(root, "access_token", response.access_token) &&
         NumberField(root, "expires_in", response.expires_in);
}

std::string EncodeTodayRequest(const TodayRequest& request) {
  std::ostringstream out;
  StartObject(out);
  out << Quote("child_id") << ':' << Quote(request.child_id);
  EndObject(out);
  return out.str();
}

bool DecodeTodayResponse(const std::string& json, TodayResponse& response) {
  JsonValue root;
  if (!ParseJson(json, root)) return false;
  if (!StringField(root, "date", response.date)) return false;
  const auto* tasks = Field(root, "tasks");
  if (!tasks || tasks->kind != JsonValue::Kind::Array) return false;
  response.tasks.clear();
  for (const auto& value : tasks->array) {
    if (value.kind != JsonValue::Kind::Object) return false;
    claw4::domain::Task task;
    std::string task_id;
    std::string status;
    int64_t estimated = 0;
    int64_t version = 0;
    if (!StringField(value, "task_id", task_id) ||
        !StringField(value, "title", task.title) ||
        !StringField(value, "subject", task.subject) ||
        !NumberField(value, "estimated_minutes", estimated) ||
        !StringField(value, "priority", task.priority) ||
        !StringField(value, "status", status) ||
        !StringField(value, "scheduled_date", task.scheduled_date) ||
        !NumberField(value, "version", version) ||
        !ParseTaskStatus(status, task.status)) {
      return false;
    }
    task.task_id.value = std::move(task_id);
    task.estimated_minutes = static_cast<int>(estimated);
    task.version = version;
    response.tasks.push_back(std::move(task));
  }
  return true;
}

std::string EncodeBatchRequest(const SyncClient::Request& request) {
  std::ostringstream out;
  StartObject(out);
  bool first = true;
  FieldPrefix(out, first, "device_id"); out << Quote(request.device_id.value);
  FieldPrefix(out, first, "last_acked_sequence"); out << request.last_acked_sequence;
  FieldPrefix(out, first, "events"); out << '[';
  for (std::size_t i = 0; i < request.events.size(); ++i) {
    if (i != 0) out << ',';
    const auto& event = request.events[i];
    StartObject(out);
    bool event_first = true;
    FieldPrefix(out, event_first, "event_id"); out << Quote(event.event_id.value);
    FieldPrefix(out, event_first, "device_id"); out << Quote(event.device_id.value);
    FieldPrefix(out, event_first, "child_id"); out << Quote(event.child_id.value);
    FieldPrefix(out, event_first, "sequence"); out << event.sequence;
    FieldPrefix(out, event_first, "timestamp"); out << event.timestamp;
    FieldPrefix(out, event_first, "timestamp_source"); out << Quote(TimestampSourceName(event.timestamp_source));
    FieldPrefix(out, event_first, "type"); out << Quote(EventTypeName(event.type));
    FieldPrefix(out, event_first, "version"); out << event.version;
    FieldPrefix(out, event_first, "payload"); out << '{';
    bool payload_first = true;
    for (const auto& item : event.payload) {
      if (!payload_first) out << ',';
      payload_first = false;
      out << Quote(item.first) << ':' << Quote(item.second);
    }
    out << '}';
    EndObject(out);
  }
  out << ']';
  EndObject(out);
  return out.str();
}

bool DecodeBatchResponse(const std::string& json, SyncClient::Response& response) {
  JsonValue root;
  if (!ParseJson(json, root)) return false;
  int64_t ack = 0;
  int64_t server_time = 0;
  if (!NumberField(root, "last_acked_sequence", ack) ||
      !NumberField(root, "server_time", server_time)) {
    return false;
  }
  const auto* results = Field(root, "results");
  if (!results || results->kind != JsonValue::Kind::Array) return false;
  response.error_class = SyncErrorClass::None;
  response.http_status = 200;
  response.batch.last_acked_sequence = ack;
  response.batch.server_time = server_time;
  response.batch.results.clear();
  for (const auto& value : results->array) {
    if (value.kind != JsonValue::Kind::Object) return false;
    PerEventResult result;
    std::string event_id;
    std::string status;
    int64_t sequence = 0;
    int64_t http_status = 0;
    if (!StringField(value, "event_id", event_id) ||
        !StringField(value, "status", status) ||
        !NumberField(value, "sequence", sequence) ||
        !NumberField(value, "http_status", http_status) ||
        !ParseEventOutcome(status, result.outcome)) {
      return false;
    }
    result.event_id.value = std::move(event_id);
    result.sequence = sequence;
    result.http_status = static_cast<int>(http_status);
    response.batch.results.push_back(std::move(result));
  }
  return true;
}

}  // namespace wire
}  // namespace sync
}  // namespace claw4
