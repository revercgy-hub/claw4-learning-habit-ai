# S-08 M1 Wake diagnostic build Candidate report

**Task:** `S-08-WAKE-DIAG-CANDIDATE-EVIDENCE`
**Branch:** `codex/v6-m1-wake-diag-candidate`
**Source checkpoint:** `85b2265b65a09b67d9eabe9da5373bf18468161b`
**Candidate:** `claw4-learning-v6-m1-wake-diag-s08-20260926-01`
**Status:** `BUILD_COMPLETE_REVIEW_REQUIRED / DEVICE_NOT_TESTED`

## Result

The interrupted S-08 candidate is now recorded as an offline build candidate. The existing fresh build tree `E:/v6/m1-wake-diag-s08` was inspected without restaging, reconfiguring, or rebuilding. Its final log segment contains `Project build complete. To flash, run:` followed by the generated flash-command help. The manifest binds source commit `85b2265b65a09b67d9eabe9da5373bf18468161b`, the pinned XiaoZhi and ESP-IDF commits, staged input hashes and source Git blob proofs, final dependency lock, sdkconfig, partition inputs, build log, and actual artifacts.

The new app is 3,136,144 bytes with SHA-256 `5619f2f2cd19cc42746fe1025a7c6390e037313573e1203093dfbf057889b27c`. The ELF is 48,036,908 bytes with SHA-256 `fc07bcb12a04b5738c1557bf1014fe39ad0659da8955c7ac5578826eb277bbb5`. The partition table binary is 3,072 bytes with SHA-256 `ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f`; it matches the previously documented live-layout comparison. The Candidate manifest SHA-256 is `0b99633818a2449dc754abd7b21d354d210877a784604ecc7aacc73cb1783a7f`. Nothing was written to the device.

## Scope and evidence checks

- **Source and stage identity:** The checkout was clean at the source checkpoint. All 11 board overlay files in the stage manifest match their recorded staged copies; LF-normalized repository bytes match the corresponding Git blobs. The manifest records the source SHA, Git blob OIDs/SHA-256, staged source input hashes, stage manifest hash, and normalized-byte rule.
- **M1 configuration:** `v6-stage-manifest.json` records `m1_input_diagnostics=true`. Final `sdkconfig` has `CONFIG_CLAW4_M1_INPUT_DIAGNOSTICS=y`, `CONFIG_CLAW4_M0_DIAGNOSTICS` unset, `CONFIG_USE_DEVICE_AEC=y`, the fixed NAS OTA URL `http://192.168.3.100:7443/xiaozhi/ota/`, and `partitions/claw4-live-m1.csv`.
- **Build inputs:** Final `dependencies.lock` is 38,913 bytes, SHA-256 `18efb5df229b4e429b9a2119a4b069346ef4a7d5ea180fdfcee97ac1c87661a2`; the seed lock is bound separately. The staged Kconfig hash is `a211512937facd081350e05bacdd719305e69a2fe8eaefcadded4db0878900c6`.
- **Build evidence:** `E:/v6/m1-wake-diag-s08-build.log` is 453,275 bytes, SHA-256 `3abeb553490d05ead890a8072e5c58f9190a9c2cb9b3d7da6136218981b806fc`. The final build segment ends in the success marker. Build artifacts and byte counts are recorded in the Candidate manifest.
- **Previous Candidate:** The endpoint Candidate `claw4-learning-v6-m1-preflight-endpoint-s04-r03-20260925-03` remains historical. No prior device result is inherited by S-08.

## Validation

`py -3.14 -B -m unittest discover -s tools/v6 -p "test_*.py"`: **125 tests, 0 failures/errors**. JSON parsing and Candidate/source/config/artifact assertions passed. Relative links in the three changed V6 documents resolve. `git diff --check`: PASS.

## Limits and risks

No COM7 or device access, flash/NVS access, staging, reconfiguration, or rebuild occurred. No Wake, Voice Preflight round, or M1 acceptance is claimed. `HARDWARE_VERIFY_REQUIRED`: the new app's startup, input diagnostics, Wake behavior, WebSocket connection, AEC effectiveness, and any Voice Preflight behavior remain unverified on device. M0 remains `CHANGES_REQUIRED`.

No scope deviation occurred. The independent review should focus on the source-to-stage identity map, Kconfig separation between M1 input counters and M0 diagnostics, final build-log success, fixed endpoint/lock inputs, and partition/artifact identity. A review pass would not itself authorize device operations.
