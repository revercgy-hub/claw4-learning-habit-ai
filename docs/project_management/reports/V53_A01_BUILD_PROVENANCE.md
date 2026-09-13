# V53 A01 Build Provenance

## Delivery

- Task: `A01` (CODEX-V53)
- Branch: `codex/v53-a01-build-provenance`
- Base: `1ae2a0c546615666a57abdf601b343bc88a043c4`
- Commit: the single commit containing this report (SHA returned with REVIEW_READY)
- Scope: repository mirror tooling, source/build hash manifest coverage, and isolated fixture tests.

## Changes

- `sync-app-first-mirror.ps1` now accepts an explicit repository root for fixture/reproducible use, includes optional `time` and `reminder` trees, and hashes before copying.
- A mismatched file gets a fresh destination mtime even when its source mtime is older, while equal content is not copied and keeps its mtime.
- Previous manifest entries are the only files eligible for stale-file removal. Unknown files in mapped directories are retained.
- The script reports missing and stale controlled CMake registrations and fails the run when registration and the current source inventory disagree. It does not edit the vendor CMake file.
- `write-app-first-manifest.ps1` includes optional time/reminder files and emits normalized lowercase SHA-256 values.
- `sync-app-first-mirror.tests.ps1` exercises equal-content mtime stability, changed-content with old source mtime, stale removal, unknown-file preservation, and CMake registration failure.

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
| Added/removed files are detectable | Hash mismatch and previous-manifest stale inventory are reported |
| Unknown files are not deleted | Fixture places `user-note.txt` beside a stale controlled file and verifies retention |
| CMake and source inventory agree | Missing and extra controlled registrations are detected; vendor CMake is never edited |
| SHA manifest covers optional modules | Mirror allowlist and candidate manifest enumerate `firmware/main/time` and `firmware/main/reminder` when present |

## Risks and boundaries

- No IDF build, device operation, Flash/NVS operation, or vendor/upstream write was performed. `E:/c` and vendor sources were not modified.
- A stale controlled CMake registration makes Sync return nonzero after performing safe file cleanup; the CMake edit remains a separately reviewed integration patch.
- Optional time/reminder directories are skipped when absent; once present, their files participate in mirror and CMake checks.
- CMake validation is source-registration validation for the controlled learning paths. It does not claim a complete vendor build or hardware result.

## Scope

No scope deviation. `HARDWARE_VERIFY_REQUIRED`: none for this host-only tooling change; device/build acceptance remains with A05 and the main agent.

## Review focus

Please review the previous-manifest deletion boundary, path normalization on Windows, CMake registration parsing, and the distinction between host tooling evidence and device build evidence.
