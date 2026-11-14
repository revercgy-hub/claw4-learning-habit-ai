#include "learning_today.h"

#ifdef CLAW4_LEARNING_PREVIEW
#include <algorithm>
#include <array>
#include <ctime>
#include <memory>
#include <mutex>
#include <set>
#include <type_traits>

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "learning_voice_mcp.h"

namespace claw4::v6 {
namespace {

constexpr size_t kMaxBody = 16 * 1024;
struct ReceivedBody {
  std::string bytes;
  bool oversized = false;
};

class TodayTransport final : public sync::HttpTransport {
 public:
  sync::HttpResponse request(const std::string& method, const std::string& url,
                             const std::vector<std::pair<std::string, std::string>>& headers,
                             const std::string& body, int64_t timeout_ms) override {
    if (!url.starts_with("https://") || (method != "GET" && method != "POST") ||
        body.size() > kMaxBody) return {};
    ReceivedBody received;
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.method = method == "GET" ? HTTP_METHOD_GET : HTTP_METHOD_POST;
    config.timeout_ms = static_cast<int>(timeout_ms);
    config.disable_auto_redirect = true;
    config.max_authorization_retries = -1;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.user_data = &received;
    config.event_handler = [](esp_http_client_event_t* event) -> esp_err_t {
      if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
      auto& output = *static_cast<ReceivedBody*>(event->user_data);
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
    if (esp_timer_get_time() / 1000 >= next_today_ms) {
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
      next_today_ms = esp_timer_get_time() / 1000 + (success ? 30'000 : 60'000);
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
