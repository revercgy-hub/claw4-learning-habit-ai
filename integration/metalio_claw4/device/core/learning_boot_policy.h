// V5.3 A02 — pure policy for deciding whether an empty learning store may
// receive the development demo snapshot.
#pragma once

namespace claw4::metalio {

enum class ProvisioningDisposition { Unprovisioned, Provisioned, InvalidOrUnavailable };
enum class LearningBootAction { SeedDemo, AwaitBackend, ClosedBootGate };
enum class LearningStorageStatus { Missing, Present, Error };

inline LearningBootAction DecideLearningBootAction(
    ProvisioningDisposition provisioning, bool has_committed_state) {
  if (provisioning == ProvisioningDisposition::InvalidOrUnavailable)
    return LearningBootAction::ClosedBootGate;
  if (has_committed_state || provisioning == ProvisioningDisposition::Provisioned)
    return LearningBootAction::AwaitBackend;
  return LearningBootAction::SeedDemo;
}

// Production boot orchestration. Callbacks are deliberately injected so the
// ordering and failure behavior can be tested without IDF/NVS.
template <typename LoadState, typename Prepare, typename Seed, typename Start>
inline bool RunLearningBoot(ProvisioningDisposition provisioning,
                            LoadState load_state, Prepare prepare,
                            Seed seed_demo, Start start_app) {
  if (provisioning == ProvisioningDisposition::InvalidOrUnavailable) return false;
  const LearningStorageStatus storage = load_state();
  if (storage == LearningStorageStatus::Error || !prepare()) return false;
  const auto action = DecideLearningBootAction(
      provisioning, storage == LearningStorageStatus::Present);
  if (action == LearningBootAction::ClosedBootGate) return false;
  if (action == LearningBootAction::SeedDemo && !seed_demo()) return false;
  start_app();
  return true;
}

}  // namespace claw4::metalio
