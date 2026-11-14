#include "learning_pairing.h"

#ifdef CLAW4_LEARNING_USB_PAIRING
#include "learning_lan_ca.inc"
#include "learning_usb_pairing.inc"

#include <atomic>
#include <cstring>
#include <string>
#include <string_view>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/usb_serial_jtag_ll.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

namespace {
constexpr char kNamespace[] = "learning_cfg";
constexpr char kKey[] = "v6_pairing";
constexpr size_t kMaxFrame = 8 + 36 + 1 + 128 + 1 + 64 + 1;
std::atomic<bool> pending{false};
std::atomic<bool> begun{false};

bool Hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }
bool Child(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '_' || c == '-'; }
bool Valid(std::string_view frame, claw4::sync::ProvisionedBackendConfig& out) {
  if (frame.size() < 8 + 36 + 1 + 1 + 1 + 64 + 1 || frame.size() > kMaxFrame ||
      frame.substr(0, 8) != "C4PAIR1 " || frame.back() != '\n') return false;
  const auto device = frame.substr(8, 36);
  constexpr int hyphens[] = {8, 13, 18, 23};
  for (int i = 0; i < 36; ++i) {
    bool hyphen = false;
    for (int at : hyphens) hyphen |= i == at;
    if (hyphen ? device[i] != '-' : !Hex(device[i])) return false;
  }
  if (device[14] != '4' || (device[19] != '8' && device[19] != '9' &&
                            device[19] != 'a' && device[19] != 'b')) return false;
  if (frame[44] != ' ') return false;
  const auto second = frame.find(' ', 45);
  if (second == std::string_view::npos || second - 45 < 1 || second - 45 > 128 ||
      second + 66 != frame.size()) return false;
  for (char c : frame.substr(45, second - 45)) if (!Child(c)) return false;
  for (char c : frame.substr(second + 1, 64)) if (!Hex(c)) return false;
  unsigned char sha[32];
  if (mbedtls_sha256(reinterpret_cast<const unsigned char*>(frame.data()), frame.size(), sha, 0) != 0) return false;
  unsigned diff = 0;
  constexpr char digits[] = "0123456789abcdef";
  if (sizeof(kLearningPairingSha256) != 65) return false;
  for (size_t i = 0; i < 32; ++i) {
    diff |= static_cast<unsigned>(digits[sha[i] >> 4] ^ kLearningPairingSha256[2 * i]);
    diff |= static_cast<unsigned>(digits[sha[i] & 15] ^ kLearningPairingSha256[2 * i + 1]);
  }
  if (diff) return false;
  out = {kLearningLanOrigin, std::string(device), std::string(frame.substr(45, second - 45)),
         std::string(frame.substr(second + 1, 64))};
  return true;
}

enum class Read { Missing, Ready, Error };
Read Load(claw4::sync::ProvisionedBackendConfig& out) {
  nvs_handle_t handle;
  const auto opened = nvs_open(kNamespace, NVS_READONLY, &handle);
  if (opened == ESP_ERR_NVS_NOT_FOUND) return Read::Missing;
  if (opened != ESP_OK) return Read::Error;
  size_t size = 0;
  const auto length = nvs_get_blob(handle, kKey, nullptr, &size);
  if (length == ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); return Read::Missing; }
  if (length != ESP_OK || size == 0 || size > kMaxFrame) { nvs_close(handle); return Read::Error; }
  std::string frame(size, '\0');
  const auto read = nvs_get_blob(handle, kKey, frame.data(), &size);
  nvs_close(handle);
  return read == ESP_OK && Valid(frame, out) ? Read::Ready : Read::Error;
}
bool Save(std::string_view frame) {
  nvs_handle_t handle;
  if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
  const auto set = nvs_set_blob(handle, kKey, frame.data(), frame.size());
  const auto commit = set == ESP_OK ? nvs_commit(handle) : set;
  nvs_close(handle);
  claw4::sync::ProvisionedBackendConfig checked;
  return commit == ESP_OK && Load(checked) == Read::Ready;
}
void Receive(void*) {
  ESP_LOGI("Claw4Pairing", "LEARNING_PAIRING_READY %s", kLearningPairingSha256);
  std::string line;
  line.reserve(kMaxFrame);
  const int64_t deadline = esp_timer_get_time() + 30000000;
  while (esp_timer_get_time() < deadline) {
    unsigned char bytes[64];
    const int count = usb_serial_jtag_ll_read_rxfifo(bytes, sizeof(bytes));
    for (int i = 0; i < count; ++i) {
      if (line.size() == kMaxFrame) { line.clear(); continue; }
      line.push_back(static_cast<char>(bytes[i]));
      if (bytes[i] == '\n') {
        claw4::sync::ProvisionedBackendConfig config;
        if (Valid(line, config) && Save(line)) {
          ESP_LOGI("Claw4Pairing", "LEARNING_PAIRING_SAVED %s", kLearningPairingSha256);
          pending.store(false);
          vTaskDelete(nullptr);
        }
        line.clear();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  pending.store(false);
  vTaskDelete(nullptr);
}
}  // namespace

void Claw4LearningPairingBegin() {
  if (begun.exchange(true)) return;
  claw4::sync::ProvisionedBackendConfig config;
  if (Load(config) != Read::Missing) return;
  // Legacy records, including partial records, are never overwritten here.
  nvs_handle_t handle;
  if (nvs_open(kNamespace, NVS_READONLY, &handle) == ESP_OK) {
    constexpr const char* keys[] = {"base_url", "device_id", "child_id", "device_secret"};
    for (const auto* key : keys) {
      size_t size = 0;
      const auto rc = nvs_get_str(handle, key, nullptr, &size);
      if (rc != ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); return; }
    }
    nvs_close(handle);
  }
  pending.store(true);
  if (xTaskCreate(Receive, "learning_pair", 4096, nullptr, 3, nullptr) != pdPASS) pending.store(false);
}
bool Claw4LearningPairingPending() { return pending.load(); }
claw4::sync::ProvisionedBackendConfig Claw4LearningPairingRead(bool& allow_legacy) {
  claw4::sync::ProvisionedBackendConfig config;
  const auto result = Load(config);
  allow_legacy = result == Read::Missing;
  return result == Read::Ready ? config : claw4::sync::ProvisionedBackendConfig{};
}
#else
void Claw4LearningPairingBegin() {}
bool Claw4LearningPairingPending() { return false; }
claw4::sync::ProvisionedBackendConfig Claw4LearningPairingRead(bool& allow_legacy) {
  allow_legacy = true;
  return {};
}
#endif
