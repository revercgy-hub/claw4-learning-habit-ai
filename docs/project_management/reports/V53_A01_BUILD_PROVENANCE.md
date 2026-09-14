# V53 A01 Build Provenance

## Delivery

- Task: `A01` (CODEX-V53)
- Branch: `codex/v53-a01-build-provenance`
- Base: `1ae2a0c546615666a57abdf601b343bc88a043c4`
- Commits: `af2daa7` (initial implementation), followed by the review-fix commit containing this report update (SHA returned with REVIEW_READY)
- Scope: repository mirror tooling, source/build hash manifest coverage, and isolated fixture tests.

## Changes

- `sync-app-first-mirror.ps1` now accepts an explicit repository root for fixture/reproducible use, includes optional `time` and `reminder` trees, and hashes before copying.
- A mismatched file gets a fresh destination mtime even when its source mtime is older, while equal content is not copied and keeps its mtime.
- Stale files are detected from the previous manifest and retained; Sync and Check fail until the source/CMake mapping is corrected. Unknown files are never deleted.
- The script validates the canonical `mirror_root`, rejects unreadable, legacy-without-root, cross-mirror, incomplete, or unsafe manifest entries, and preserves stale records across repeated failures.
- CMake validation strips CMake line, C-style, and bracket comments, then only parses static source arguments in `idf_component_register(SRCS ...)`, `set(SOURCES ...)`, and `list(APPEND SOURCES ...)`. It does not treat `message(...)` or filename prefixes as registration.
- `write-app-first-manifest.ps1` includes optional time/reminder files and emits normalized lowercase SHA-256 values.
- `sync-app-first-mirror.tests.ps1` exercises equal-content mtime stability, changed-content with old source mtime, persistent stale failure, unknown-file preservation, unsafe manifest paths, optional modules, and comment/prefix CMake false positives.

## Verification

Command:

```powershell
pwsh -NoProfile -File tools/dev/tests/sync-app-first-mirror.tests.ps1
```

Result: exit code `0`, `sync-app-first-mirror fixture tests: PASS`.

The fixture output also recorded `mismatch_before=0` on the equal-content run, `mismatch_before=1` after content change, `cmake_extra=...` after controlled source removal, and `cmake_missing=...` for an omitted registration. The negative CMake checks are expected and are asserted by the fixture.

Manifest smoke check (temporary output directory):

```powershell
pwsh -NoProfile -File tools/dev/write-app-first-manifest.ps1 -RepoRoot (Get-Location).Path -OutputDir <temporary-directory>
```

Result: exit code `0`; source hashes and byte counts emitted. No generated manifest was added to the repository.

## Acceptance self-check

| Criterion | Evidence |
| --- | --- |
| Same content does not trigger rebuild timestamp churn | Fixture compares destination mtime across a second Sync |
| Older source mtime cannot hide changed content | Fixture changes content, sets source mtime to 2020, and verifies destination mtime advances |
| Added/removed files are detectable | Hash mismatch and persistent previous-manifest stale inventory are reported; stale files are not auto-deleted |
| Unknown files are not deleted | Fixture places `user-note.txt` beside a stale controlled file and verifies retention |
| CMake and source inventory agree | Missing and extra controlled registrations are detected from supported static declarations; vendor CMake is never edited |
| SHA manifest covers optional modules | Mirror allowlist and candidate manifest enumerate `firmware/main/time` and `firmware/main/reminder` when present |

## Risks and boundaries

- No IDF build, device operation, Flash/NVS operation, or vendor/upstream write was performed. `E:/c` and vendor sources were not modified.
- A stale controlled CMake registration makes Sync return nonzero; the CMake edit remains a separately reviewed integration patch.
- Optional time/reminder directories are skipped when absent; once present, their files participate in mirror and CMake checks.
- CMake validation is source-registration validation for the controlled learning paths. It does not claim a complete vendor build or hardware result.

## Scope

Scope deviation: review fixes intentionally remove the earlier auto-delete behavior and add manifest/CMake safety gates. `HARDWARE_VERIFY_REQUIRED`: none for this host-only tooling change; device/build acceptance remains with A05 and the main agent.

## Review focus

## Fixed upstream patch evidence

The pinned upstream reference is `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`. Read-only `git show` from `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4` was successful. The reproducible upstream commit patch is stored at `tools/dev/patches/upstream-ca3aa3fa.patch` (generated from first parent to the pinned merge and limited to `main/CMakeLists.txt` and `main/display/lv_adapter_display.cc`; apply/reverse must be run in a parent fixture). Against first parent, the merge changes `main/CMakeLists.txt` (11 added lines) and `main/display/lv_adapter_display.cc` (2 added, 1 removed); the four-file read-only comparison against `E:/c` recorded `27/0`, `22/1`, `16/3`, and `10/3` added/removed lines respectively for `home_screen.cc`, `CMakeLists.txt`, `lv_adapter_display.cc`, and `audio_service.cc`. Blob SHA-1s at the pinned commit are: `home_screen.cc` `27f55af84e2740cc0f82d406d9711f695b322df8`, `CMakeLists.txt` `577bd14d859aa0cf1861608f88341af52606410a`, `lv_adapter_display.cc` `5ab93fe7f8f03b45132960869acaec42a2326ae1`, `audio_service.cc` `2e24746587804d9031249427ae60a15bf692a309`. No vendor file was modified. The comparison is evidence of source differences, not a claim of a completed IDF build.

Please review the fixed manifest mapping boundary, path normalization on Windows, CMake registration parsing, and the distinction between host tooling evidence and vendor/device build evidence.
