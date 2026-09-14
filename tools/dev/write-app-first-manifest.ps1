# CODEX-APP-FIRST-001 AF3b — produce a candidate source/build manifest.
[CmdletBinding()]
param(
    [string]$RepoRoot = "",
    [string]$BuildBinary = "",
    [string]$OutputDir = "out\app-first\manifest"
)

$ErrorActionPreference = "Stop"
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
$outDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $RepoRoot $OutputDir }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$git = git -C $RepoRoot -c safe.directory=$RepoRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or -not $git) { throw "git provenance unavailable; refusing to write manifest" }
$paths = @(
    "firmware/main/sync/wire_codec.h",
    "firmware/main/sync/wire_codec.cpp",
    "firmware/main/sync/backend_client.h",
    "firmware/main/sync/backend_client.cpp",
    "firmware/main/sync/backend_sync_transport.h",
    "firmware/main/ui/sync_diagnostics.h",
    "firmware/main/ui/sync_diagnostics.cpp",
    "integration/metalio_claw4/integration_manifest.md"
)
# Keep the provenance manifest aligned with the mirror allowlist, including
# runtime/screen sources and optional time/reminder modules.
$roots = @("firmware/main/learning_domain", "firmware/main/sync", "firmware/main/application", "firmware/main/interaction", "firmware/main/mcp", "firmware/main/ui", "firmware/main/ports", "firmware/main/time", "firmware/main/reminder", "integration/metalio_claw4/host_glue", "integration/metalio_claw4/device/core", "integration/metalio_claw4/device/ports", "integration/metalio_claw4/device/app", "integration/metalio_claw4/device/learning_screen")
foreach ($root in $roots) {
    $full = Join-Path $RepoRoot $root
    if (Test-Path -LiteralPath $full) { $paths += Get-ChildItem -LiteralPath $full -File -Recurse | ForEach-Object { [IO.Path]::GetRelativePath($RepoRoot, $_.FullName).Replace('\', '/') } }
}
$patchRoot = Join-Path $PSScriptRoot 'patches'
if (Test-Path -LiteralPath $patchRoot) { $paths += Get-ChildItem -LiteralPath $patchRoot -File -Filter '*.patch' | ForEach-Object { [IO.Path]::GetRelativePath($RepoRoot, $_.FullName).Replace('\', '/') } }
$files = @()
foreach ($relative in $paths) {
    $full = Join-Path $RepoRoot $relative
    if (-not (Test-Path -LiteralPath $full)) { throw "manifest input missing: $relative" }
    $hash = Get-FileHash -LiteralPath $full -Algorithm SHA256
    $files += [ordered]@{ path = $relative; sha256 = $hash.Hash.ToLowerInvariant(); bytes = (Get-Item -LiteralPath $full).Length }
}
$binary = $null
if ($BuildBinary) {
    if (-not (Test-Path -LiteralPath $BuildBinary)) { throw "build binary missing: $BuildBinary" }
    $hash = Get-FileHash -LiteralPath $BuildBinary -Algorithm SHA256
    $binary = [ordered]@{ path = $BuildBinary; sha256 = $hash.Hash.ToLowerInvariant(); bytes = (Get-Item -LiteralPath $BuildBinary).Length }
}
$manifest = [ordered]@{
    task = "CODEX-APP-FIRST-001"
    checkpoint = "AF3b"
    git_commit = $git.Trim()
    generated_at = (Get-Date).ToString("o")
    git_dirty = [bool](git -C $RepoRoot -c safe.directory=$RepoRoot status --porcelain)
    source_files = $files
    build_binary = $binary
    forbidden_operations = @("flash", "erase_flash", "partition_change", "bootloader_change", "ota_1_change", "eFuse")
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outDir "app-first-manifest.json") -Encoding utf8
$manifest | ConvertTo-Json -Depth 8
