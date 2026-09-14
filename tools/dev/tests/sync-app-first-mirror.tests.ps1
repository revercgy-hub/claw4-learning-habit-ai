$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot '..\sync-app-first-mirror.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('claw4-a01-' + [guid]::NewGuid().ToString('N'))
$mirror = Join-Path $root 'mirror'
$manifest = Join-Path $root 'manifest.json'
New-Item -ItemType Directory -Force -Path $root, (Join-Path $mirror 'main') | Out-Null
$required = @(
  'firmware/main/learning_domain', 'firmware/main/sync', 'firmware/main/application',
  'firmware/main/interaction', 'firmware/main/mcp', 'firmware/main/ui', 'firmware/main/ports',
  'integration/metalio_claw4/host_glue', 'integration/metalio_claw4/device/core',
  'integration/metalio_claw4/device/ports', 'integration/metalio_claw4/device/app',
  'integration/metalio_claw4/device/learning_screen')
foreach ($dir in $required) { New-Item -ItemType Directory -Force -Path (Join-Path $root $dir) | Out-Null }
New-Item -ItemType Directory -Force -Path (Join-Path $root 'firmware/main/time'), (Join-Path $root 'firmware/main/reminder') | Out-Null
Set-Content -LiteralPath (Join-Path $root 'firmware/main/learning_domain/reducer.cpp') -Value 'old-content'
Set-Content -LiteralPath (Join-Path $root 'firmware/main/sync/wire_codec.cpp') -Value 'codec'
Set-Content -LiteralPath (Join-Path $root 'integration/metalio_claw4/device/app/runtime.cpp') -Value 'runtime'
Set-Content -LiteralPath (Join-Path $root 'firmware/main/time/time.cpp') -Value 'time'
Set-Content -LiteralPath (Join-Path $root 'firmware/main/reminder/reminder.cpp') -Value 'reminder'
$cmake = @'
idf_component_register(
  # "learning/time/time.cpp" is comment-only and must not count
  SRCS "learning/learning_domain/reducer.cpp"
        "learning/sync/wire_codec.cpp"
        "learning/metalio_claw4/device/app/runtime.cpp"
        "learning/time/time.cpp"
        "learning/reminder/reminder.cpp"
)
'@
Set-Content -LiteralPath (Join-Path $mirror 'main/CMakeLists.txt') -Value $cmake

& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Sync -ManifestPath $manifest
if ($LASTEXITCODE -ne 0) { throw 'initial sync failed' }
$target = Join-Path $mirror 'main/learning/learning_domain/reducer.cpp'
$oldStamp = (Get-Item -LiteralPath $target).LastWriteTimeUtc
Start-Sleep -Milliseconds 1200
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Sync -ManifestPath $manifest
if ($LASTEXITCODE -ne 0) { throw 'same-content sync failed' }
if ((Get-Item -LiteralPath $target).LastWriteTimeUtc -ne $oldStamp) { throw 'same content changed target mtime' }

Set-Content -LiteralPath (Join-Path $root 'firmware/main/learning_domain/reducer.cpp') -Value 'new-content'
(Get-Item -LiteralPath (Join-Path $root 'firmware/main/learning_domain/reducer.cpp')).LastWriteTimeUtc = [datetime]::Parse('2020-01-01T00:00:00Z')
Start-Sleep -Milliseconds 1200
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Sync -ManifestPath $manifest
if ($LASTEXITCODE -ne 0) { throw 'changed-content sync failed' }
if ((Get-Content -Raw -LiteralPath $target) -ne "new-content`r`n") { throw 'changed content was not copied' }
if ((Get-Item -LiteralPath $target).LastWriteTimeUtc -le $oldStamp) { throw 'changed content did not refresh mtime' }

$unknown = Join-Path $mirror 'main/learning/learning_domain/user-note.txt'
$cmakePath = Join-Path $mirror 'main/CMakeLists.txt'
Set-Content -LiteralPath $unknown -Value 'keep me'
Remove-Item -LiteralPath (Join-Path $root 'firmware/main/learning_domain/reducer.cpp')
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Sync -ManifestPath $manifest
if ($LASTEXITCODE -eq 0) { throw 'stale CMake registration was not detected' }
if (-not (Test-Path -LiteralPath $target)) { throw 'stale controlled file was unexpectedly deleted' }
if (-not (Test-Path -LiteralPath $unknown)) { throw 'unknown file was removed' }
$cmake = @'
idf_component_register(SRCS "learning/learning_domain/reducer.cpp"
        "learning/sync/wire_codec.cpp"
        "learning/metalio_claw4/device/app/runtime.cpp"
        "learning/time/time.cpp" "learning/reminder/reminder.cpp")
'@
Set-Content -LiteralPath $cmakePath -Value $cmake
Set-Content -LiteralPath (Join-Path $root 'firmware/main/learning_domain/reducer.cpp') -Value 'restored'
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Sync -ManifestPath $manifest
if ($LASTEXITCODE -ne 0) { throw 'stale manifest did not recover after source/CMake restoration' }
Remove-Item -LiteralPath (Join-Path $root 'firmware/main/learning_domain/reducer.cpp'), $target
$cmake = $cmake.Replace('idf_component_register(SRCS "learning/learning_domain/reducer.cpp"', 'idf_component_register(SRCS')
Set-Content -LiteralPath $cmakePath -Value $cmake
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Check -ManifestPath $manifest
if ($LASTEXITCODE -ne 0) { throw 'cleaned source/mirror/CMake removal did not recover' }
$sentinel = Join-Path $root 'sentinel.txt'; Set-Content -LiteralPath $sentinel -Value 'outside'
$badManifest = Join-Path $root 'bad-manifest.json'
Set-Content -LiteralPath $badManifest -Value '{"files":[{"repo_path":"firmware/main/sync/wire_codec.cpp","mirror_path":"../sentinel.txt"}]}'
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Sync -ManifestPath $badManifest
if ($LASTEXITCODE -eq 0 -or (Get-Content -Raw -LiteralPath $sentinel) -ne "outside`r`n") { throw 'unsafe manifest path was not blocked' }
$otherMirror = Join-Path $root 'other-mirror'; New-Item -ItemType Directory -Force -Path (Join-Path $otherMirror 'main') | Out-Null
Copy-Item -LiteralPath (Join-Path $mirror 'main/CMakeLists.txt') -Destination (Join-Path $otherMirror 'main/CMakeLists.txt')
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $otherMirror -Mode Check -ManifestPath $manifest
if ($LASTEXITCODE -eq 0) { throw 'manifest for a different mirror was accepted' }

Set-Content -LiteralPath $cmakePath -Value @'
idf_component_register(SRCS "learning/sync/wire_codec.cpp" "learning/sync/wire_codec.cpp.bak")
'@
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Check -ManifestPath $manifest
if ($LASTEXITCODE -eq 0) { throw 'missing CMake registration was not detected' }
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Check -ManifestPath $manifest
if ($LASTEXITCODE -eq 0) { throw 'stale/missing CMake failure did not persist on repeat' }
$validCmake = @'
idf_component_register(SRCS "learning/sync/wire_codec.cpp"
 "learning/metalio_claw4/device/app/runtime.cpp" "learning/time/time.cpp"
 "learning/reminder/reminder.cpp")
'@
Set-Content -LiteralPath $cmakePath -Value $validCmake
# A real older manifest has no stale_manifest_records property.
$legacy = Get-Content -Raw -LiteralPath $manifest | ConvertFrom-Json
$legacy.PSObject.Properties.Remove('stale_manifest_records')
$legacy | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifest
& pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Check -ManifestPath $manifest
if ($LASTEXITCODE -ne 0) { throw 'old manifest without optional stale property rejected' }
foreach ($kind in @('comment', 'message', 'prefix', 'include', 'unused-variable')) {
    $missing = $validCmake.Replace('"learning/time/time.cpp"', '')
    switch ($kind) {
        'comment' { $missing += "`n#[=[ learning/time/time.cpp ]=]" }
        'message' { $missing += '`nmessage("learning/time/time.cpp")' }
        'prefix' { $missing = $missing.Replace('SRCS', 'SRCS "learning/time/time.cpp.bak"') }
        'include' { $missing = $missing.Replace(')', ' PRIV_INCLUDE_DIRS "learning/time/time.cpp")') }
        'unused-variable' { $missing += '`nset(SOURCES "learning/time/time.cpp")' }
    }
    Set-Content -LiteralPath $cmakePath -Value $missing
    & pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Check -ManifestPath $manifest
    if ($LASTEXITCODE -eq 0) { throw "CMake false registration accepted: $kind" }
}
Set-Content -LiteralPath $cmakePath -Value $validCmake
# Exercise the path rejection itself, with a valid mirror binding.
foreach ($badPath in @('../sentinel.txt', $sentinel)) {
    @{ mirror_root = $mirror; files = @(@{repo_path='firmware/main/sync/wire_codec.cpp'; mirror_path=$badPath}) } |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $badManifest
    & pwsh -NoProfile -File $script -RepoRoot $root -MirrorRoot $mirror -Mode Sync -ManifestPath $badManifest
    if ($LASTEXITCODE -eq 0 -or (Get-Content -Raw -LiteralPath $sentinel).Trim() -ne 'outside') {
        throw 'unsafe path with valid mirror root was not rejected'
    }
}
Write-Output 'sync-app-first-mirror fixture tests: PASS'
