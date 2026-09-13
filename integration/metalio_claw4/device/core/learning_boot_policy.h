// V5.3 A02 — pure policy for deciding whether an empty learning store may
// receive the development demo snapshot.
#pragma once

namespace claw4::metalio {

enum class ProvisioningDisposition { Unprovisioned, Provisioned, InvalidOrUnavailable };
enum class LearningBootAction { SeedDemo, AwaitBackend, ClosedBootGate };

inline LearningBootAction DecideLearningBootAction(
    ProvisioningDisposition provisioning, bool has_committed_state) {
  if (provisioning == ProvisioningDisposition::InvalidOrUnavailable)
    return LearningBootAction::ClosedBootGate;
  if (has_committed_state || provisioning == ProvisioningDisposition::Provisioned)
    return LearningBootAction::AwaitBackend;
  return LearningBootAction::SeedDemo;
}

}  // namespace claw4::metalio
