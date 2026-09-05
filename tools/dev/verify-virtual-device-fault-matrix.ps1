# CODEX-APP-FIRST-001 AF2 — host Virtual Device offline/restart/fault matrix.
[CmdletBinding()]
param(
    [string]$CompilerPath = "",
    [string]$OutputDir = "out\app-first\fault-matrix"
)

$ErrorActionPreference = "Stop"
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { $PWD.Path }
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$outDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Reuse the canonical runner build and baseline scenario.
& (Join-Path $repoRoot "tools\dev\verify-virtual-device-app.ps1") `
    -CompilerPath $CompilerPath -OutputDir $outDir | Out-Null
$exe = Join-Path $outDir "virtual_device_app.exe"

$scenario = @"
boot
seed
touch start:demo-math-001
network offline
sync
network lost
sync
sync
restart
touch complete:demo-math-001
network duplicate
sync
summary
quit
"@
$lines = @($scenario | & $exe 2>&1)
$runExit = $LASTEXITCODE
$lines | Set-Content -LiteralPath (Join-Path $outDir "fault-matrix.jsonl") -Encoding utf8
if ($runExit -ne 0) { throw "fault matrix runner failed (exit=$runExit)" }
$records = @($lines | Where-Object { $_ -is [string] -and $_.Trim() } |
    ForEach-Object { $_ | ConvertFrom-Json })

$offline = $records | Where-Object { $_.op -eq "sync_backoff" } | Select-Object -First 1
$recovered = $records | Where-Object { $_.op -eq "sync_synced" } | Select-Object -Last 1
$restart = $records | Where-Object { $_.op -eq "restart" } | Select-Object -Last 1
$summary = $records | Where-Object { $_.op -eq "summary" } | Select-Object -Last 1
if (-not $offline -or $offline.pending -ne 2) { throw "offline sync did not retain pending=2" }
if (-not $recovered -or $recovered.pending -ne 0) { throw "recovery sync did not converge pending=0" }
if (-not $restart -or $restart.active -ne "demo-math-001") { throw "restart did not recover active session" }
if (-not $summary -or $summary.pending -ne 0 -or $null -ne $summary.active) {
    throw "duplicate recovery summary is not clean"
}

$result = [ordered]@{
    task = "CODEX-APP-FIRST-001"
    checkpoint = "AF2"
    passed = $true
    scenario = (Join-Path $outDir "fault-matrix.jsonl")
    offlinePending = $offline.pending
    restartActive = $restart.active
    finalPending = $summary.pending
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outDir "fault-matrix-summary.json") -Encoding utf8
$result | ConvertTo-Json -Depth 5
