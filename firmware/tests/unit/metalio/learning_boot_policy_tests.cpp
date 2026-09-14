#include <cstdio>

#include "metalio_claw4/device/core/learning_boot_policy.h"

using claw4::metalio::DecideLearningBootAction;
using claw4::metalio::LearningBootAction;
using claw4::metalio::ProvisioningDisposition;
using claw4::metalio::LearningStorageStatus;

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
  int loads = 0;
  int prepares = 0;
  int seeds = 0;
  int starts = 0;
  const bool first_boot_started = claw4::metalio::RunLearningBoot(
      ProvisioningDisposition::Unprovisioned,
      [&] { ++loads; return LearningStorageStatus::Missing; },
      [&] { ++prepares; return true; },
      [&] { ++seeds; return true; }, [&] { ++starts; });
  const bool provisioned_waited = claw4::metalio::RunLearningBoot(
      ProvisioningDisposition::Provisioned,
      [&] { ++loads; return LearningStorageStatus::Missing; },
      [&] { ++prepares; return true; },
      [&] { ++seeds; return true; }, [&] { ++starts; });
  const bool seed_failure_closed = !claw4::metalio::RunLearningBoot(
      ProvisioningDisposition::Unprovisioned,
      [&] { ++loads; return LearningStorageStatus::Missing; },
      [&] { ++prepares; return true; },
      [&] { ++seeds; return false; }, [&] { ++starts; });
  const bool invalid_rejected_before_load = !claw4::metalio::RunLearningBoot(
      ProvisioningDisposition::InvalidOrUnavailable,
      [&] { ++loads; return LearningStorageStatus::Present; },
      [&] { ++prepares; return true; }, [&] { ++seeds; return true; },
      [&] { ++starts; });
  const bool read_failure_closed = !claw4::metalio::RunLearningBoot(
      ProvisioningDisposition::Unprovisioned,
      [&] { ++loads; return LearningStorageStatus::Error; },
      [&] { ++prepares; return true; }, [&] { ++seeds; return true; },
      [&] { ++starts; });
  const bool prepare_failure_closed = !claw4::metalio::RunLearningBoot(
      ProvisioningDisposition::Unprovisioned,
      [&] { ++loads; return LearningStorageStatus::Missing; },
      [&] { ++prepares; return false; }, [&] { ++seeds; return true; },
      [&] { ++starts; });
  const bool orchestration_ok = first_boot_started && provisioned_waited &&
      seed_failure_closed && invalid_rejected_before_load && read_failure_closed &&
      prepare_failure_closed && loads == 5 && prepares == 4 && seeds == 2 &&
      starts == 2;
  const bool ok = fresh_unpaired && provisioned_empty && formal_state_preserved &&
                  unpaired_state_preserved && failed_read_closed && orchestration_ok;
  std::printf("learning_boot_policy_tests: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
