# Check or synchronize the allowlisted App-first sources into an IDF mirror.
# The repository is authoritative. This script never deletes files and never
# edits sdkconfig, partition tables, bootloader, BSP, or driver sources.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MirrorRoot,

    [ValidateSet('Check', 'Sync')]
    [string]$Mode = 'Check',

    [string]$ManifestPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$mirror = (Resolve-Path -LiteralPath $MirrorRoot).Path
$cmake = Join-Path $mirror 'main/CMakeLists.txt'
if (-not (Test-Path -LiteralPath $cmake)) {
    throw "MirrorRoot must contain main/CMakeLists.txt: $mirror"
}

if (-not $ManifestPath) {
    $ManifestPath = Join-Path $repoRoot 'out/app-first-mirror-manifest.json'
}

function Add-Tree([System.Collections.Generic.List[object]]$entries, [string]$sourceRoot, [string]$targetRoot) {
    $fullSource = Join-Path $repoRoot $sourceRoot
    if (-not (Test-Path -LiteralPath $fullSource)) {
        throw "Allowlisted source tree is missing: $sourceRoot"
    }
    Get-ChildItem -LiteralPath $fullSource -File -Recurse | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($fullSource, $_.FullName)
        $entries.Add([pscustomobject]@{
            RepoPath = (Join-Path $sourceRoot $relative).Replace('\', '/')
            MirrorPath = (Join-Path $targetRoot $relative).Replace('\', '/')
        })
    }
}

$entries = [System.Collections.Generic.List[object]]::new()
foreach ($tree in @(
    @('firmware/main/learning_domain', 'main/learning/learning_domain'),
    @('firmware/main/sync', 'main/learning/sync'),
    @('firmware/main/application', 'main/learning/application'),
    @('firmware/main/interaction', 'main/learning/interaction'),
    @('firmware/main/mcp', 'main/learning/mcp'),
    @('firmware/main/ui', 'main/learning/ui'),
    @('firmware/main/ports', 'main/learning/ports'),
    @('integration/metalio_claw4/host_glue', 'main/learning/metalio_claw4/host_glue'),
    @('integration/metalio_claw4/device/core', 'main/learning/metalio_claw4/device/core'),
    @('integration/metalio_claw4/device/ports', 'main/learning/metalio_claw4/device/ports'),
    @('integration/metalio_claw4/device/app', 'main/learning/metalio_claw4/device/app'),
    @('integration/metalio_claw4/device/learning_screen', 'main/display/screen/learning_screen')
)) {
    Add-Tree $entries $tree[0] $tree[1]
}

$records = [System.Collections.Generic.List[object]]::new()
$mismatches = [System.Collections.Generic.List[object]]::new()
foreach ($entry in $entries | Sort-Object RepoPath) {
    $source = Join-Path $repoRoot $entry.RepoPath
    $target = Join-Path $mirror $entry.MirrorPath
    $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()
    $targetExists = Test-Path -LiteralPath $target
    $targetHash = if ($targetExists) { (Get-FileHash -Algorithm SHA256 -LiteralPath $target).Hash.ToLowerInvariant() } else { $null }
    $record = [pscustomobject]@{
        repo_path = $entry.RepoPath
        mirror_path = $entry.MirrorPath
        bytes = (Get-Item -LiteralPath $source).Length
        repo_sha256 = $sourceHash
        mirror_sha256_before = $targetHash
        equal_before = ($targetExists -and $sourceHash -eq $targetHash)
    }
    $records.Add($record)
    if (-not $record.equal_before) { $mismatches.Add($record) }
}

if ($Mode -eq 'Sync') {
    foreach ($record in $mismatches) {
        $target = Join-Path $mirror $record.mirror_path
        $parent = Split-Path -Parent $target
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        Copy-Item -LiteralPath (Join-Path $repoRoot $record.repo_path) -Destination $target -Force
    }
    foreach ($record in $records) {
        $target = Join-Path $mirror $record.mirror_path
        $record | Add-Member -NotePropertyName mirror_sha256_after -NotePropertyValue ((Get-FileHash -Algorithm SHA256 -LiteralPath $target).Hash.ToLowerInvariant())
        $record | Add-Member -NotePropertyName equal_after -NotePropertyValue ($record.repo_sha256 -eq $record.mirror_sha256_after)
    }
} else {
    foreach ($record in $records) {
        $record | Add-Member -NotePropertyName mirror_sha256_after -NotePropertyValue $record.mirror_sha256_before
        $record | Add-Member -NotePropertyName equal_after -NotePropertyValue $record.equal_before
    }
}

$manifestParent = Split-Path -Parent $ManifestPath
New-Item -ItemType Directory -Force -Path $manifestParent | Out-Null
$payload = [ordered]@{
    generated_at = (Get-Date).ToUniversalTime().ToString('o')
    mode = $Mode
    repo_root = $repoRoot
    mirror_root = $mirror
    allowlist_count = $records.Count
    mismatch_count_before = $mismatches.Count
    mismatch_count_after = @($records | Where-Object { -not $_.equal_after }).Count
    files = @($records)
}
$payload | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Output ("mirror={0} mode={1} files={2} mismatch_before={3} mismatch_after={4}" -f $mirror, $Mode, $records.Count, $mismatches.Count, $payload.mismatch_count_after)
if ($payload.mismatch_count_after -ne 0) {
    $records | Where-Object { -not $_.equal_after } | Select-Object repo_path, mirror_path, repo_sha256, mirror_sha256_after | Format-Table -AutoSize
    exit 1
}
exit 0
