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
- Added a read-only `loadWithPresence()` result (`Missing`/`Present`/`Error`) so boot policy performs one NVS read and cannot turn a second failed presence probe into an empty store. Provisioning and learning state failures are rejected before `prepareAfterBoot()`.

## Verification

- `git diff --check` — PASS.
- `rg -n "ResetToSeed|SelfTest|SELFTEST" integration/metalio_claw4/device` — no production references remain.
- `tools/dev/verify-host-cpp-tests.ps1 -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe -CrossCompilerPath E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe -OutputDir out/host-tests-a02-r6` — PASS, `unit summary : 19 / 19 PASS`, interface contract exit 0, final exit 0.
- The gate executed `learning_boot_policy_tests` (PASS), the real `LearningApp` glue suite (7 cases PASS), codec/restart recovery, provisioning, coordinator, and all other registered suites.
- The real LearningApp regression covers ACK=1 with pending events retained, formal Touch Complete, completed state and pending/sequence surviving reconstruction over the same FakeDisk, plus injected commit failure remaining atomic. The production-used boot orchestration helper is tested for fresh unpaired seed, provisioned empty wait, seed failure, invalid provisioning rejection before load/prepare, read failure, and prepare failure.

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

The provisioning and outbox adapter changes are narrow supporting changes required to distinguish missing namespaces from actual read/open failures. No device, NVS erase, database, voice, or backend sequence operation was run. No hardware claim is made; `HARDWARE_VERIFY_REQUIRED` remains applicable to any later device build/boot verification.

## Review focus

Please inspect the startup policy mapping, the NVS error classification, and the absence of all reset/self-test call paths. Re-run the global Host gate with the configured compiler and the metalio policy test before integration.
