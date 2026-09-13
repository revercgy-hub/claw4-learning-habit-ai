#include <cstdio>

#include "metalio_claw4/device/core/learning_boot_policy.h"

using claw4::metalio::DecideLearningBootAction;
using claw4::metalio::LearningBootAction;
using claw4::metalio::ProvisioningDisposition;

int main() {
  const bool fresh_unpaired =
      DecideLearningBootAction(ProvisioningDisposition::Unprovisioned, false) ==
      LearningBootAction::SeedDemo;
  const bool provisioned_empty =
      DecideLearningBootAction(ProvisioningDisposition::Provisioned, false) ==
      LearningBootAction::AwaitBackend;
  const bool formal_state_preserved =
      DecideLearningBootAction(ProvisioningDisposition::Provisioned, true) ==
      LearningBootAction::AwaitBackend;
  const bool unpaired_state_preserved =
      DecideLearningBootAction(ProvisioningDisposition::Unprovisioned, true) ==
      LearningBootAction::AwaitBackend;
  const bool failed_read_closed =
      DecideLearningBootAction(ProvisioningDisposition::InvalidOrUnavailable,
                               false) == LearningBootAction::ClosedBootGate;
  const bool ok = fresh_unpaired && provisioned_empty && formal_state_preserved &&
                  unpaired_state_preserved && failed_read_closed;
  std::printf("learning_boot_policy_tests: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
