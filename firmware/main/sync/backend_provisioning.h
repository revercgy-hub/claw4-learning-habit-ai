// CODEX-APP-FIRST-002 P1 — device backend provisioning contract.
//
// The portable layer carries no NVS/ESP-IDF dependency. A platform adapter
// supplies the persisted endpoint, identity and per-device secret. Callers
// must never render or log device_secret.
#pragma once

#include <string>

namespace claw4::sync {

struct ProvisionedBackendConfig {
  std::string base_url;
  std::string device_id;
  std::string child_id;
  std::string device_secret;

  bool complete() const {
    return !base_url.empty() && !device_id.empty() && !child_id.empty() &&
           !device_secret.empty();
  }
};

enum class ProvisioningStatus {
  Ready,
  NotConfigured,
  Invalid,
  StorageError,
};

class BackendProvisioning {
 public:
  virtual ~BackendProvisioning() = default;

  virtual ProvisioningStatus load(ProvisionedBackendConfig& out) = 0;
  virtual ProvisioningStatus save(const ProvisionedBackendConfig& config) = 0;
  virtual ProvisioningStatus clear() = 0;
};

}  // namespace claw4::sync

