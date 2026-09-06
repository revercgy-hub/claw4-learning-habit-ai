#pragma once

#include "sync/backend_provisioning.h"

namespace claw4::sync {

class FakeBackendProvisioning final : public BackendProvisioning {
 public:
  ProvisionedBackendConfig config;
  bool has_config = false;
  bool fail_load = false;
  bool fail_save = false;

  ProvisioningStatus load(ProvisionedBackendConfig& out) override {
    if (fail_load) return ProvisioningStatus::StorageError;
    if (!has_config) return ProvisioningStatus::NotConfigured;
    out = config;
    return out.complete() ? ProvisioningStatus::Ready
                          : ProvisioningStatus::Invalid;
  }

  ProvisioningStatus save(const ProvisionedBackendConfig& next) override {
    if (fail_save) return ProvisioningStatus::StorageError;
    if (!next.complete()) return ProvisioningStatus::Invalid;
    config = next;
    has_config = true;
    return ProvisioningStatus::Ready;
  }

  ProvisioningStatus clear() override {
    config = {};
    has_config = false;
    return ProvisioningStatus::NotConfigured;
  }
};

}  // namespace claw4::sync

