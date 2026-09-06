#pragma once

#include "sync/backend_provisioning.h"

namespace claw4::metalio {

// MVP storage adapter. It uses a dedicated namespace so ResetToSeed() cannot
// delete credentials together with learning state. Production secure storage
// remains a separate hardening task; no secret is logged or exposed in UI.
class NvsBackendProvisioning final : public claw4::sync::BackendProvisioning {
 public:
  claw4::sync::ProvisioningStatus load(
      claw4::sync::ProvisionedBackendConfig& out) override;
  claw4::sync::ProvisioningStatus save(
      const claw4::sync::ProvisionedBackendConfig& config) override;
  claw4::sync::ProvisioningStatus clear() override;
};

}  // namespace claw4::metalio

