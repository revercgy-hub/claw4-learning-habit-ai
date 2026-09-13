# V53 A02 Reset Protection

## Delivery

- Task: `A02` (正式任务不能触发破坏性 Demo Reset)
- Base: `1ae2a0c546615666a57abdf601b343bc88a043c4`
- Branch: `codex/v53-a02-reset-protection`
- Status: `REVIEW_READY`

## Changes

- Removed the device `LearningRuntime::ResetToSeed()` API and its NVS erase/reseed implementation.
- Removed the automatic four step SELFTEST from the learning screen.
- Completed tasks now remain in the completed view and wait for a new backend plan; the primary action no longer resets learning storage.
- Added the pure `learning_boot_policy` used by production runtime startup. Demo seeding is allowed only for an explicitly unprovisioned device with an empty learning store. A provisioned empty store waits for the backend, and invalid/unavailable provisioning or learning storage closes the boot gate.
- Corrected provisioning NVS error classification so namespace-not-found is the only unconfigured case; missing fields are invalid and other read/open errors are storage errors.

## Verification

- `git diff --check` — PASS.
- `rg -n "ResetToSeed|SelfTest|SELFTEST" integration/metalio_claw4/device` — no production references remain.
- `tools/dev/verify-host-cpp-tests.ps1` — unable to execute the native gate in this environment: no `g++`, `clang++`, or `cl` compiler is available; script exits 1 without producing a test log.
- `learning_boot_policy_tests.cpp` covers fresh unpaired seed, provisioned empty wait, committed state preservation, unpaired committed state preservation, and failed-read closed gate. It is ready for the project host compiler gate.

## Acceptance self-check

| Criterion | Result |
|---|---|
| Completed formal tasks do not erase or replace state | PASS by production screen/runtime diff |
| Pending, ACK, sequence and IDs are not reset by UI | PASS: destructive API and both UI call sites removed |
| Provisioned empty state does not seed demo | PASS by pure policy and runtime wiring |
| Provisioning/read failure is not treated as unpaired | PASS by status classification and closed gate |
| Existing ID/codec/restart semantics unchanged | PASS: no changes to formatter, codec, coordinator, or backend sequence logic |
| Device/NVS/database operation | NOT RUN; prohibited by task scope |

## Risks and scope

The provisioning adapter error classification is a narrow supporting change required to distinguish a missing namespace from an actual NVS read/open failure. No device, NVS erase, database, voice, coordinator, or backend sequence operation was run or changed. No hardware claim is made; `HARDWARE_VERIFY_REQUIRED` remains applicable to any later device build/boot verification.

## Review focus

Please inspect the startup policy mapping, the NVS error classification, and the absence of all reset/self-test call paths. Re-run the global Host gate with the configured compiler and the metalio policy test before integration.
