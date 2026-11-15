#include "learning_today.h"

#ifdef CLAW4_LEARNING_PREVIEW
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <set>
#include <type_traits>
#include <strings.h>

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "backend_http_time.h"
#include "learning_today_retry.h"
#include "learning_voice_mcp.h"
#include "timer_preview.h"

namespace claw4::v6 {
namespace {

#ifdef CLAW4_LEARNING_LAN_CA
#include "learning_lan_ca.inc"

bool UsesLanCa(std::string_view url) {
  const std::string_view origin{kLearningLanOrigin};
  return url == origin || (url.starts_with(origin) && url.size() > origin.size() && url[origin.size()] == '/');
}
#endif

constexpr size_t kMaxBody = 16 * 1024;
struct ClockHeaders {
  std::array<char, 29> date{};
  bool has_date = false;
  bool invalid = false;

  void Read(const char* key, const char* value) {
    if (!key || !value) return;
    if (strcasecmp(key, "Age") == 0) invalid = true;
    if (strcasecmp(key, "Date") != 0) return;
    if (has_date || std::strlen(value) != date.size()) {
      invalid = true;
      return;
    }
    std::copy_n(value, date.size(), date.data());
    has_date = true;
  }
};

struct ReceivedBody {
  std::string bytes;
  bool oversized = false;
  ClockHeaders* clock_headers = nullptr;
};

class TodayTransport final : public sync::HttpTransport {
 public:
  std::optional<BackendTimeSample> SampleClock(const std::string& origin) {
#ifdef CLAW4_LEARNING_LAN_CA
    if (origin == kLearningLanOrigin) {
      ClockHeaders clock;
      const auto started = esp_timer_get_time();
      const auto response = Request("GET", origin + "/health", {{"Cache-Control", "no-cache"}}, "", 4'000, &clock);
      const auto elapsed = (esp_timer_get_time() - started + 999) / 1000;
      if (response.transport_ok && response.status == 200 && clock.has_date && !clock.invalid) {
        return ParseBackendTime({clock.date.data(), clock.date.size()}, elapsed);
      }
    }
#else
    (void)origin;
#endif
    return std::nullopt;
  }

  sync::HttpResponse request(const std::string& method, const std::string& url,
                             const std::vector<std::pair<std::string, std::string>>& headers,
                             const std::string& body, int64_t timeout_ms) override {
    return Request(method, url, headers, body, timeout_ms, nullptr);
  }

 private:
  sync::HttpResponse Request(const std::string& method, const std::string& url,
                            const std::vector<std::pair<std::string, std::string>>& headers,
                            const std::string& body, int64_t timeout_ms, ClockHeaders* clock_headers) {
    if (!url.starts_with("https://") || (method != "GET" && method != "POST") ||
        body.size() > kMaxBody) return {};
    ReceivedBody received;
    received.clock_headers = clock_headers;
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.method = method == "GET" ? HTTP_METHOD_GET : HTTP_METHOD_POST;
    config.timeout_ms = static_cast<int>(timeout_ms);
    config.disable_auto_redirect = true;
    config.max_authorization_retries = -1;
#ifdef CLAW4_LEARNING_LAN_CA
    if (UsesLanCa(url)) {
      config.cert_pem = kLearningLanCa;
      config.cert_len = sizeof(kLearningLanCa);
    } else {
      config.crt_bundle_attach = esp_crt_bundle_attach;
    }
#else
    config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    config.user_data = &received;
    config.event_handler = [](esp_http_client_event_t* event) -> esp_err_t {
      auto& output = *static_cast<ReceivedBody*>(event->user_data);
      if (event->event_id == HTTP_EVENT_ON_HEADER && output.clock_headers)
        output.clock_headers->Read(event->header_key, event->header_value);
      if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
      if (event->data_len <= 0) return ESP_OK;
      if (output.oversized || static_cast<size_t>(event->data_len) > kMaxBody - output.bytes.size()) {
        output.oversized = true;
        return ESP_FAIL;
      }
      output.bytes.append(static_cast<const char*>(event->data), event->data_len);
      return ESP_OK;
    };
    using Client = std::remove_pointer_t<esp_http_client_handle_t>;
    std::unique_ptr<Client, decltype(&esp_http_client_cleanup)> client(
        esp_http_client_init(&config), esp_http_client_cleanup);
    if (!client) return {};
    for (const auto& [key, value] : headers) {
      if (esp_http_client_set_header(client.get(), key.c_str(), value.c_str()) != ESP_OK) return {};
    }
    if (method == "POST" &&
        esp_http_client_set_post_field(client.get(), body.data(), static_cast<int>(body.size())) != ESP_OK) return {};
    if (esp_http_client_perform(client.get()) != ESP_OK || received.oversized) return {};
    return {true, esp_http_client_get_status_code(client.get()), std::move(received.bytes)};
  }
};

class Worker {
 public:
  Worker(sync::ProvisionedBackendConfig config, std::function<void(LearningTodayUpdate)> apply,
         LearningUploadResult uploaded)
      : config(std::move(config)), apply(std::move(apply)), uploaded(std::move(uploaded)) {}

  TaskHandle_t task = nullptr;
  std::mutex mailbox_mutex;
  std::optional<application::SyncRequestEnvelope> mailbox;

  void Run();

 private:
  sync::ProvisionedBackendConfig config;
  std::function<void(LearningTodayUpdate)> apply;
  LearningUploadResult uploaded;

  void PublishAuthorizationLost() {
    Application::GetInstance().Schedule([apply = apply] { apply({}); });
  }
};

void Worker::Run() {
  TodayTransport transport;
  sync::BackendClient client(transport, config.base_url, 4000);
  auto sign = CreateLearningVoiceSigner(std::move(config.device_secret));
  const auto authenticate = [&](sync::SyncErrorClass& error) {
    sync::wire::AuthResponse auth;
    return !client.accessToken().empty() ||
        client.authenticate(config.device_id, [&](const auto& device, const auto& id, const auto& nonce) {
          return sign(device + "|" + id + "|" + nonce);
        }, auth, error);
  };
  int64_t next_today_ms = 0;
#ifdef CLAW4_LEARNING_LAN_CA
  std::optional<int64_t> next_clock_ms;
  if (config.base_url == kLearningLanOrigin) next_clock_ms = 0;
#endif
  LearningTodayRetry retry;
  for (;;) {
    std::optional<application::SyncRequestEnvelope> upload;
    {
      std::lock_guard lock(mailbox_mutex);
      upload.swap(mailbox);
    }
    if (upload) {
      sync::SyncClient::Response response;
      if (authenticate(response.error_class)) response = client.syncBatch(upload->request);
      if (response.error_class == sync::SyncErrorClass::Auth) {
        client.clearAuth();
        PublishAuthorizationLost();
        next_today_ms = esp_timer_get_time() / 1000 + 5'000;
      }
      Application::GetInstance().Schedule(
          [done = uploaded, sent = std::move(*upload), result = std::move(response)]() mutable {
            done(std::move(sent), std::move(result));
          });
    }
#ifdef CLAW4_LEARNING_LAN_CA
    if (next_clock_ms && esp_timer_get_time() / 1000 >= *next_clock_ms) {
      const auto clock = transport.SampleClock(config.base_url);
      if (clock) Claw4TimerPreviewAcceptBackendTime(clock->epoch_ms, clock->uncertainty_ms);
      next_clock_ms = esp_timer_get_time() / 1000 + (clock ? 15 * 60'000 : 30'000);
    }
#endif
    // Until the clock anchors, any task snapshot fetched here would be discarded by ApplyToday.
    if (esp_timer_get_time() / 1000 >= next_today_ms && Claw4TimerPreviewTrustedTime()) {
      sync::SyncErrorClass error{};
      sync::wire::TodayResponse today;
      const bool success = authenticate(error) && client.fetchToday(config.child_id, today, error);
      if (success) {
        Application::GetInstance().Schedule([apply = apply, today = std::move(today)]() mutable {
          apply({std::move(today)});
        });
      } else if (error == sync::SyncErrorClass::Auth) {
        client.clearAuth();
        PublishAuthorizationLost();
      }
      using Result = LearningTodayRetry::Result;
      const auto result = success ? Result::Updated :
          error == sync::SyncErrorClass::Network ? Result::Unreachable : Result::Rejected;
      next_today_ms = esp_timer_get_time() / 1000 + retry.DelayAfter(result);
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1'000));
  }
}

Worker* worker = nullptr;  // Registered and queued only by the Application task.

}  // namespace

bool LearningTodayDateMatches(std::string_view date, int64_t trusted_epoch_ms) {
  if (trusted_epoch_ms <= 0 || trusted_epoch_ms >= 4102444800000LL) return false;
  // V6 NAS and timer use Asia/Shanghai for the active learning profile.
  const std::time_t local_seconds = trusted_epoch_ms / 1000 + 8 * 3600;
  std::tm local{};
  if (!gmtime_r(&local_seconds, &local)) return false;
  std::array<char, 16> iso{};
  return std::strftime(iso.data(), iso.size(), "%Y-%m-%d", &local) && date == iso.data();
}

bool LearningTodayMatches(const sync::wire::TodayResponse& today, const domain::ChildId& child,
                          int64_t trusted_epoch_ms) {
  if (!LearningTodayDateMatches(today.date, trusted_epoch_ms) || today.tasks.size() > 32) return false;
  std::set<std::string_view> ids;
  return std::all_of(today.tasks.begin(), today.tasks.end(), [&](const auto& task) {
    const auto& id = task.task_id.value;
    return task.child_id == child && !id.empty() && id.size() <= 128 &&
           id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
               std::string::npos &&
           ids.insert(id).second && !task.title.empty() && task.title.size() <= 160 &&
           task.estimated_minutes > 0 && task.estimated_minutes <= 600 &&
           task.version >= 0 && task.scheduled_date == today.date;
  });
}

bool StartLearningTodayWorker(sync::ProvisionedBackendConfig config,
                              std::function<void(LearningTodayUpdate)> apply,
                              LearningUploadResult uploaded) {
  if (worker || !apply || !uploaded || !config.complete() || config.device_secret.size() < 32 ||
      !config.base_url.starts_with("https://") ||
      config.base_url.find_first_of("@?#\\\r\n\t ") != std::string::npos ||
      config.child_id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
          std::string::npos) return false;
  worker = new Worker(std::move(config), std::move(apply), std::move(uploaded));
  if (xTaskCreate([](void* argument) {
        static_cast<Worker*>(argument)->Run();
      }, "claw4_today", 8192, worker, 2, &worker->task) == pdPASS) return true;
  delete worker;
  worker = nullptr;
  return false;
}

bool QueueLearningUpload(application::SyncRequestEnvelope envelope) {
  if (!worker || envelope.request.events.empty()) return false;
  {
    std::lock_guard lock(worker->mailbox_mutex);
    if (worker->mailbox) return false;
    worker->mailbox = std::move(envelope);
  }
  xTaskNotifyGive(worker->task);
  return true;
}

}  // namespace claw4::v6
#endif
