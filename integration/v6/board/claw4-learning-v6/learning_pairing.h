#pragma once

#include "sync/backend_provisioning.h"

// Called once after NVS initialization. Pending blocks only learning Runtime creation.
void Claw4LearningPairingBegin();
bool Claw4LearningPairingPending();
// Invalid scoped data never falls back to legacy credentials.
claw4::sync::ProvisionedBackendConfig Claw4LearningPairingRead(bool& allow_legacy);
