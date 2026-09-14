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
    ,[string]$RepoRoot
)

$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = Join-Path $PSScriptRoot '../..' }
$repoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$mirror = (Resolve-Path -LiteralPath $MirrorRoot).Path
$cmake = Join-Path $mirror 'main/CMakeLists.txt'
if (-not (Test-Path -LiteralPath $cmake)) {
    throw "MirrorRoot must contain main/CMakeLists.txt: $mirror"
}

if (-not $ManifestPath) {
    $ManifestPath = Join-Path $repoRoot 'out/app-first-mirror-manifest.json'
}

function Add-Tree([System.Collections.Generic.List[object]]$entries, [string]$sourceRoot, [string]$targetRoot, [bool]$optional = $false) {
    $fullSource = Join-Path $repoRoot $sourceRoot
    if (-not (Test-Path -LiteralPath $fullSource)) {
        if ($optional) { return }
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
$trees = @(
    [pscustomobject]@{ source = 'firmware/main/learning_domain'; target = 'main/learning/learning_domain'; optional = $false },
    [pscustomobject]@{ source = 'firmware/main/sync'; target = 'main/learning/sync'; optional = $false },
    [pscustomobject]@{ source = 'firmware/main/application'; target = 'main/learning/application'; optional = $false },
    [pscustomobject]@{ source = 'firmware/main/interaction'; target = 'main/learning/interaction'; optional = $false },
    [pscustomobject]@{ source = 'firmware/main/mcp'; target = 'main/learning/mcp'; optional = $false },
    [pscustomobject]@{ source = 'firmware/main/ui'; target = 'main/learning/ui'; optional = $false },
    [pscustomobject]@{ source = 'firmware/main/ports'; target = 'main/learning/ports'; optional = $false },
    [pscustomobject]@{ source = 'firmware/main/time'; target = 'main/learning/time'; optional = $true },
    [pscustomobject]@{ source = 'firmware/main/reminder'; target = 'main/learning/reminder'; optional = $true },
    [pscustomobject]@{ source = 'integration/metalio_claw4/host_glue'; target = 'main/learning/metalio_claw4/host_glue'; optional = $false },
    [pscustomobject]@{ source = 'integration/metalio_claw4/device/core'; target = 'main/learning/metalio_claw4/device/core'; optional = $false },
    [pscustomobject]@{ source = 'integration/metalio_claw4/device/ports'; target = 'main/learning/metalio_claw4/device/ports'; optional = $false },
    [pscustomobject]@{ source = 'integration/metalio_claw4/device/app'; target = 'main/learning/metalio_claw4/device/app'; optional = $false },
    [pscustomobject]@{ source = 'integration/metalio_claw4/device/learning_screen'; target = 'main/display/screen/learning_screen'; optional = $false }
)
foreach ($tree in $trees) {
    Add-Tree $entries $tree.source $tree.target $tree.optional
}

# Only files recorded by a previous manifest are eligible for removal. This
# prevents an unrelated file placed in a mapped directory from being deleted.
$previous = @{}
if (Test-Path -LiteralPath $ManifestPath) {
    try { $old = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json }
    catch { throw "Manifest is unreadable; refusing to continue: $ManifestPath" }
    if (-not $old.mirror_root) { throw "Manifest has no mirror_root; refusing unsafe migration: $ManifestPath" }
    $oldMirror = [IO.Path]::GetFullPath([string]$old.mirror_root).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $canonicalMirror = $mirror.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    if (-not $oldMirror.Equals($canonicalMirror, [StringComparison]::OrdinalIgnoreCase)) { throw "Manifest belongs to a different mirror; refusing to continue: $oldMirror" }
    foreach ($f in @($old.files) + @($old.stale_manifest_records)) {
        if ($null -eq $f) { continue }
        if (-not $f.repo_path -or -not $f.mirror_path) { throw "Manifest entry is incomplete; refusing to continue: $ManifestPath" }
        $mp = ([string]$f.mirror_path).Replace('\', '/')
        $rp = ([string]$f.repo_path).Replace('\', '/')
        if ([IO.Path]::IsPathRooted($mp) -or $mp -match '(^|/)\.\.?(/|$)' -or [IO.Path]::IsPathRooted($rp) -or $rp -match '(^|/)\.\.?(/|$)') { throw "Manifest contains unsafe path; refusing to continue: $mp" }
        $mapped = $false
        foreach ($tree in $trees) {
            $targetPrefix = $tree.target.TrimEnd('/') + '/'; $sourcePrefix = $tree.source.TrimEnd('/') + '/'
            if ($mp.StartsWith($targetPrefix, [StringComparison]::OrdinalIgnoreCase) -and $rp.StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase)) {
                $suffixM = $mp.Substring($targetPrefix.Length); $suffixR = $rp.Substring($sourcePrefix.Length)
                if ($suffixM -eq $suffixR) { $mapped = $true }
            }
        }
        if (-not $mapped) { throw "Manifest mapping is outside the fixed allowlist; refusing to continue: $mp -> $rp" }
        $targetFull = [IO.Path]::GetFullPath((Join-Path $mirror $mp))
        $mirrorPrefix = $mirror.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        if (-not $targetFull.StartsWith($mirrorPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Manifest target escapes mirror; refusing to continue: $mp" }
        $previous[$mp] = $rp
    }
}
$currentByMirror = @{}
foreach ($entry in $entries) { $currentByMirror[$entry.MirrorPath] = $entry.RepoPath }
$stale = @($previous.Keys | Where-Object { -not $currentByMirror.ContainsKey($_) -and (Test-Path -LiteralPath (Join-Path $mirror $_)) })

function Get-CmakeSource([string]$mirrorPath) {
    if (-not ($mirrorPath -match '^main/(.*)$')) { return $null }
    return $Matches[1]
}
$cmakeText = Get-Content -Raw -LiteralPath $cmake
$cmakeCode = [regex]::Replace($cmakeText, '(?s)#\[=*\[.*?\]\=*\]', '')
$cmakeCode = [regex]::Replace($cmakeCode, '(?s)/\*.*?\*/', '')
$cmakeCode = [regex]::Replace($cmakeCode, '(?m)#.*$', '')
function Test-CmakeToken([string]$text, [string]$token) {
    return $text -match ('(?<![A-Za-z0-9_./-])' + [regex]::Escape($token) + '(?![A-Za-z0-9_./-])')
}
$registrationCode = @([regex]::Matches($cmakeCode, '(?is)(?:idf_component_register\s*\(|set\s*\(\s*SOURCES\b|list\s*\(\s*APPEND\s+SOURCES\b)') | ForEach-Object {
    $tail = $cmakeCode.Substring($_.Index + $_.Length); $close = $tail.IndexOf(')'); if ($close -ge 0) {
        $body = $tail.Substring(0, $close)
        if ($_.Value -match '(?i)idf_component_register') {
            $sr = [regex]::Match($body, '(?is)\bSRCS\b(.*?)(?=\b(?:INCLUDE_DIRS|PRIV_INCLUDE_DIRS|REQUIRES|PRIV_REQUIRES|SRC_DIRS|EXCLUDE_SRCS|WHOLE_ARCHIVE|EMBED_FILES|EMBED_TXTFILES)\b|$)'); if ($sr.Success) { $sr.Groups[1].Value } else { throw "idf_component_register has no statically parseable SRCS argument" }
        } else { $body }
    }
}) -join "`n"
$cmakeMissing = @($entries | Where-Object { $_.MirrorPath -match '\.(cpp|cc|c)$' } | ForEach-Object {
    $source = Get-CmakeSource $_.MirrorPath
    if ($source -and -not (Test-CmakeToken $registrationCode $source)) { $_.MirrorPath }
})
$registeredSources = @([regex]::Matches($registrationCode, '(?<![A-Za-z0-9_./-])((?:learning|display/screen/learning_screen)/[^\s()"]+\.(?:cpp|cc|c))(?![A-Za-z0-9_./-])') | ForEach-Object { $_.Groups[1].Value.Replace('\', '/') })
$expectedSources = @($entries | Where-Object { $_.MirrorPath -match '\.(cpp|cc|c)$' } | ForEach-Object { (Get-CmakeSource $_.MirrorPath) })
$cmakeExtra = @($registeredSources | Where-Object { $_ -notin $expectedSources })

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
        # Copy-Item can preserve an old source timestamp on some providers.
        # A content change must invalidate downstream build dependencies even
        # when the source checkout clock is older than the mirror.
        (Get-Item -LiteralPath $target).LastWriteTimeUtc = [DateTime]::UtcNow
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
    stale_manifest_count = $stale.Count
    stale_manifest_paths = @($stale)
    stale_manifest_records = @($old.files + $old.stale_manifest_records | Where-Object { $_.mirror_path -in $stale })
    cmake_missing_count = $cmakeMissing.Count
    cmake_missing_sources = @($cmakeMissing)
    cmake_extra_count = $cmakeExtra.Count
    cmake_extra_sources = @($cmakeExtra)
    files = @($records)
}
$payload | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Output ("mirror={0} mode={1} files={2} mismatch_before={3} mismatch_after={4}" -f $mirror, $Mode, $records.Count, $mismatches.Count, $payload.mismatch_count_after)
if ($payload.mismatch_count_after -ne 0 -or $payload.stale_manifest_count -ne 0 -or $payload.cmake_missing_count -ne 0 -or $payload.cmake_extra_count -ne 0) {
    $records | Where-Object { -not $_.equal_after } | Select-Object repo_path, mirror_path, repo_sha256, mirror_sha256_after | Format-Table -AutoSize
    if ($payload.cmake_missing_count -ne 0) { Write-Output ("cmake_missing=" + ($payload.cmake_missing_sources -join ',')) }
    if ($payload.cmake_extra_count -ne 0) { Write-Output ("cmake_extra=" + ($payload.cmake_extra_sources -join ',')) }
    if ($payload.stale_manifest_count -ne 0) { Write-Output ("stale_manifest=" + ($payload.stale_manifest_paths -join ',')) }
    exit 1
}
exit 0
