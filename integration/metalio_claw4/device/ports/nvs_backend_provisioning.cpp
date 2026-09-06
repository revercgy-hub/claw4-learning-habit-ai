#include "metalio_claw4/device/ports/nvs_backend_provisioning.h"

#include <array>
#include <utility>

#include "nvs.h"

namespace claw4::metalio {
namespace {

constexpr const char* kNamespace = "learning_cfg";
constexpr const char* kBaseUrl = "base_url";
constexpr const char* kDeviceId = "device_id";
constexpr const char* kChildId = "child_id";
constexpr const char* kSecret = "device_secret";
constexpr size_t kMaxFieldBytes = 256;

bool ReadString(nvs_handle_t handle, const char* key, std::string& out) {
  size_t len = 0;
  if (nvs_get_str(handle, key, nullptr, &len) != ESP_OK || len == 0 ||
      len > kMaxFieldBytes) {
    return false;
  }
  std::string value(len, '\0');
  if (nvs_get_str(handle, key, value.data(), &len) != ESP_OK) return false;
  value.resize(len > 0 ? len - 1 : 0);
  out = std::move(value);
  return true;
}

}  // namespace

claw4::sync::ProvisioningStatus NvsBackendProvisioning::load(
    claw4::sync::ProvisionedBackendConfig& out) {
  nvs_handle_t handle;
  if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
    return claw4::sync::ProvisioningStatus::NotConfigured;
  }
  claw4::sync::ProvisionedBackendConfig candidate;
  const bool ok = ReadString(handle, kBaseUrl, candidate.base_url) &&
                  ReadString(handle, kDeviceId, candidate.device_id) &&
                  ReadString(handle, kChildId, candidate.child_id) &&
                  ReadString(handle, kSecret, candidate.device_secret);
  nvs_close(handle);
  if (!ok) return claw4::sync::ProvisioningStatus::NotConfigured;
  out = std::move(candidate);
  return out.complete() ? claw4::sync::ProvisioningStatus::Ready
                        : claw4::sync::ProvisioningStatus::Invalid;
}

claw4::sync::ProvisioningStatus NvsBackendProvisioning::save(
    const claw4::sync::ProvisionedBackendConfig& config) {
  if (!config.complete() || config.base_url.size() > kMaxFieldBytes ||
      config.device_id.size() > kMaxFieldBytes ||
      config.child_id.size() > kMaxFieldBytes ||
      config.device_secret.size() > kMaxFieldBytes) {
    return claw4::sync::ProvisioningStatus::Invalid;
  }
  nvs_handle_t handle;
  if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    return claw4::sync::ProvisioningStatus::StorageError;
  }
  const esp_err_t set_base = nvs_set_str(handle, kBaseUrl, config.base_url.c_str());
  const esp_err_t set_device = nvs_set_str(handle, kDeviceId, config.device_id.c_str());
  const esp_err_t set_child = nvs_set_str(handle, kChildId, config.child_id.c_str());
  const esp_err_t set_secret = nvs_set_str(handle, kSecret, config.device_secret.c_str());
  const esp_err_t commit = (set_base == ESP_OK && set_device == ESP_OK &&
                            set_child == ESP_OK && set_secret == ESP_OK)
                               ? nvs_commit(handle)
                               : ESP_FAIL;
  nvs_close(handle);
  return commit == ESP_OK ? claw4::sync::ProvisioningStatus::Ready
                          : claw4::sync::ProvisioningStatus::StorageError;
}

claw4::sync::ProvisioningStatus NvsBackendProvisioning::clear() {
  nvs_handle_t handle;
  if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    return claw4::sync::ProvisioningStatus::StorageError;
  }
  const esp_err_t erase = nvs_erase_all(handle);
  const esp_err_t commit = erase == ESP_OK ? nvs_commit(handle) : erase;
  nvs_close(handle);
  return commit == ESP_OK ? claw4::sync::ProvisioningStatus::NotConfigured
                          : claw4::sync::ProvisioningStatus::StorageError;
}

}  // namespace claw4::metalio
