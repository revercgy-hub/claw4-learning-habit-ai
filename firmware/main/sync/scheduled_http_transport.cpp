#include "sync/scheduled_http_transport.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace claw4::sync {
namespace {
struct State {
  std::mutex mutex;
  std::condition_variable ready;
  bool done = false;
  HttpResponse response;
};
}  // namespace

HttpResponse ScheduledHttpTransport::request(
    const std::string& method, const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body, const int64_t timeout_ms) {
  if (!schedule_ || !request_ || timeout_ms < 0) return {};
  auto state = std::make_shared<State>();
  schedule_([state, method, url, headers, body, timeout_ms, request = request_]() {
    const HttpResponse response = request(method, url, headers, body, timeout_ms);
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->response = response;
      state->done = true;
    }
    state->ready.notify_one();
  });

  std::unique_lock<std::mutex> lock(state->mutex);
  const auto wait_ms = std::chrono::milliseconds(timeout_ms + 1000);
  if (!state->ready.wait_for(lock, wait_ms, [&] { return state->done; })) return {};
  return state->response;
}

}  // namespace claw4::sync
