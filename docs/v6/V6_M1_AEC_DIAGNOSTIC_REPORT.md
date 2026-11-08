# L-36 S-24 AEC diagnostic candidate evidence

> Candidate build and independent R-18 review are complete for the exact manifest below. This is not a device test, AEC result, or authorization to flash.

## Task and scope

- Task: `L-36-M1-AEC-CANDIDATE-EVIDENCE` (docs-only evidence capture).
- Branch: `codex/l36-aec-evidence`.
- Source baseline: `5a9dbfcf66ad6d8c505a8d3be8126e8362d1fb03` (R-17 reviewed integration baseline).
- Documentation review based on source integration `5a9dbfcf66ad6d8c505a8d3be8126e8362d1fb03`.
- Allowed files: this report, `docs/v6/V6_TASK_BOARD.md`, and `docs/project_management/TASK_BOARD.md` only.
- Scope: record the opt-in diagnostic candidate evidence and its limits. No source edits, build/flash/device operations, raw log or transcript inclusion, NAS/SSH access, or claims about acoustic delay, AFE feed, or AEC success.

## Source change inspected

S-24 `de9e6903fd80d2ccaf59c87968d3399f31f36089` follows `c9a641ed128fa249288ee9fb4a8cf70f379e3c93`. Its source diff shows:

- Codec diagnostics are opt-in and report aggregate `M1_AUDIO_WINDOW` / `M1_AUDIO_ENERGY` snapshots at intervals of at least one second, using ESP timer timestamps. Counters and energy totals describe the observed software paths; they do not establish acoustic alignment, speaker onset, or AFE feed quality.
- AFE AEC diagnostics report `M1_AFE_AEC` with a coherent snapshot of requested state, apply count, latest raw return code, and timestamp. The apply count saturates at `UINT32_MAX`.
- These are diagnostic observations only. They do not assert that AEC works, that TX reference reaches the AFE, or that a delay is calibrated.
- Real zero reference and reference shortage are represented separately. The existing incomplete-input guard is unaffected.
- No sample data is logged.

L-35 parser correction `d44f06f104bfe7ab3d899a2cfda0bf4867e2f7bf` is evidence context: it fixes Wake counts to recognize the exact reviewed detector strings. Reported checks: targeted parser `22/22`, full L-35 suite `202/202`. Root safe replay used the same capture hash `a6546d6d282d5ba3be0c583fa9a12eed6e197cc15838be7ee4a672dd0dcb7803`; only `m1_wake_log_count` changed `0→2`, producing safe replay SHA-256 `43925ba9b13b9d3456d74aa1dee2de624cdc350409d44f4d9dec87e9c3a5edb3`.

S-24 reported full suite `199/199` and C++17 `-Wall -Wextra -Werror` pass. Integrated context reports `202/202` and the same C++ warning-clean pass. These are source/host checks, not build or device evidence for the candidate below.

## Candidate build and review

- Candidate: `claw4-learning-v6-m1-aec-diag-s24-20261108-01`.
- Source and reviewed-source SHA in manifest: `5a9dbfcf66ad6d8c505a8d3be8126e8362d1fb03`.
- Build status: `BUILT_NOT_FLASHED`; `flash_performed=false`; device validation `NOT_VERIFIED`.
- Safe manifest: `E:/v6/m1-aec-s24-20261108-01-evidence/candidate-manifest.json`, SHA-256 `8c082e7199e81561d93d648e17abe6e0f9d76aa0043282bddb3e51b5d7f45fbe` (recomputed; 8,696 bytes).
- App image: 3,138,496 bytes, SHA-256 `b58e027c6b92baa14f98831317556843ef46b83feb94c835aae3423504f3310d` (recomputed).
- ELF: 48,081,004 bytes, SHA-256 `8f73018b6011053efa877c7678dcce05cbae112fb640954a6adc0d13a30c68c6` (recomputed); manifest records the app's embedded ELF hash as an exact full match.
- Manifest records upstream XiaoZhi `ac6deed3d8e75348475364bf40ad953c6cd48054` and ESP-IDF `fff9895c82d744c7237be8847347bdd1b07c6643`. sdkconfig bytes match prior S15 exactly; generated partition and model match S15. This is identity evidence, not device evidence.
- Build recovery: an initial compiler pass was followed by a strict post-local-override lock guard rejection of five registry upgrades (`dl_fft 0.7.0→0.8.0`, `esp_hosted 2.12.13→2.13.1`, `esp_sccb_intf 0.0.9→1.0.0`, `iot_usbh_cdc 3.1.0→3.1.1`, `usb 1.5.0→1.6.0`). A clean retry still selected registry Wi-Fi and was rejected. The successful isolated build used exact reviewed defaults and pins plus a staged-only upstream manifest `override_path` to the existing guarded local Wi-Fi package (`~3.3.1`); the project lock-policy/build script were unchanged. `staged-wifi-selection.json` and `configure-precheck.json` document the selection and guards. The lock otherwise matches the prior reviewed file. The recovery changed only inputs inside the isolated build tree.
- Build and independent root hash freeze reported exit 0. R-18 independently reviewed and **PASS** for the exact manifest SHA above. It verified independent artifact hashes, embedded ELF identity, exact S15 sdkconfig/model/partition identity, strict lock with only a manifest-hash delta, staged exact Wi-Fi override, and diagnostic strings.
- The bootloader output differs from S15. It is recorded only as a build artifact and is excluded from any permitted write payload. The app image is at `0x200000` and is under 4 MiB. This report does not authorize any write operation.

The candidate is built and R-18 reviewed. It remains unflashed and device validation remains `NOT_VERIFIED`. No Flash or device test is authorized by this document.

## Current evidence boundary and status

The most recent historical device status remains L-34 S-17 Preflight: `PREFLIGHT_CHANGES_REQUIRED / TTS_RECAPTURE_OBSERVED`, credited full rounds `0/2–3`; formal 20 rounds `NOT_STARTED`; M0 `CHANGES_REQUIRED`; AP outage/recovery `SKIPPED_BY_USER / NOT_VERIFIED`. L-34's observed recapture does not establish physical root cause or general AEC/near-end behavior. The diagnostic candidate may support later same-candidate playback-only and near-end contrasts after separate authorization and gate review; it does not itself change those statuses.

Unknowns remain: physical acoustic delay/alignment, whether the reference is effectively fed to AFE, AEC effectiveness, near-end behavior, and any relationship between aggregate software counters and actual acoustic events. No gain/delay tuning, server-side muting, or uplink gating is proposed here.

## Acceptance checklist

- [x] Changes limited to the three authorized documentation paths.
- [x] Exact source diff semantics recorded without claiming AEC pass or acoustic delay.
- [x] L-35 parser evidence distinguished from candidate build/device evidence.
- [x] Root build result and candidate identity recorded from manifest (successful isolated build; see reproduction inputs below).
- [x] Manifest and app/ELF hashes recomputed from the safe evidence directory/build output.
- [x] Independent R-18 review `PASS` recorded for the exact manifest and artifact identity.
- [x] Both task boards synchronized with evidence and explicit limitations.
- [x] `git diff --check` passed and the commit contains only allowlisted paths.

## Validation and reproduction

- Worktree base check: `git rev-parse HEAD` → `5a9dbfcf66ad6d8c505a8d3be8126e8362d1fb03`.
- Source review: `git diff c9a641ed128fa249288ee9fb4a8cf70f379e3c93 de9e6903fd80d2ccaf59c87968d3399f31f36089 --`.
- Metadata hash checks: candidate manifest, app image, and ELF recomputed and match manifest; R-18 independently verified exact artifact identity.
- Documentation validation: report links and evidence hashes matched; `git diff --check` and allowlist checks passed.
- L-36 committed the evidence report and two task boards.

The isolated stage was created with `py -3.14 -B tools/v6/stage_device.py --variant m1 --m1-input-diagnostics` using the pinned XiaoZhi checkout and the candidate build root. The private evidence directory contains the hash-guarded `pin_staged_wifi_selection.py`, `run_pinned_configure_build.py` (configure check then build with the pinned ESP-IDF Python), and `verify_candidate.py` (manifest freeze). Use the exact source SHA, generated defaults, pinned package inputs, and staged Wi-Fi selection recorded above.

## Remaining risks / hardware verification

`HARDWARE_VERIFY_REQUIRED`: any claim about acoustic alignment, AFE reference feed, AEC effectiveness, playback-only suppression, near-end behavior, or device response. Candidate has not been flashed; no device validation is represented. Device actions remain outside this task and require their own authorization and reviewed candidate.

## Review focus

Confirm that the report preserves source/host/build/device separation, makes no Flash authorization inference, does not upgrade historical `0/2–3`, M0, AP, or AEC statuses, and describes the diagnostic only as aggregate evidence.
