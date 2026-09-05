# CODEX-APP-FIRST-001 AF0 — build and exercise the real LearningApp runner.

[CmdletBinding()]
param(
    [string]$CompilerPath = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { $PWD.Path }
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$outDir = if ($OutputDir) { $OutputDir } else { Join-Path $repoRoot "out\app-first\virtual-device" }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (-not $CompilerPath) {
    $candidate = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($candidate) { $CompilerPath = $candidate.Source }
}
if (-not $CompilerPath -or -not (Test-Path -LiteralPath $CompilerPath)) {
    throw "A native C++17 compiler is required. Pass -CompilerPath."
}
$compilerDir = Split-Path -Parent $CompilerPath
$env:PATH = "$compilerDir;$env:PATH"

$includeMain = Join-Path $repoRoot "firmware\main"
$includeTests = Join-Path $repoRoot "firmware\tests"
$includeIntegration = Join-Path $repoRoot "integration"
$source = Join-Path $repoRoot "firmware\tests\host\virtual_device_app.cpp"
$impl = @(
    (Join-Path $repoRoot "firmware\main\learning_domain\reducer.cpp"),
    (Join-Path $repoRoot "firmware\main\sync\outbox_core.cpp"),
    (Join-Path $repoRoot "firmware\main\application\coordinator.cpp"),
    (Join-Path $repoRoot "firmware\main\interaction\dispatcher.cpp"),
    (Join-Path $repoRoot "firmware\main\mcp\learning_mcp_host.cpp"),
    (Join-Path $repoRoot "firmware\main\ui\presenters.cpp"
))
$exe = Join-Path $outDir "virtual_device_app.exe"
$compileArgs = @(
    "-std=c++17", "-Wall", "-Wextra", "-Werror",
    "-I", $includeMain, "-I", $includeTests, "-I", $includeIntegration,
    $source
)
$compileArgs += $impl
$compileArgs += @("-o", $exe)
$compileOutput = & $CompilerPath @compileArgs 2>&1
$compileExit = $LASTEXITCODE
if ($compileOutput) { $compileOutput | Set-Content -LiteralPath (Join-Path $outDir "compile.log") -Encoding utf8 }
if ($compileExit -ne 0 -or -not (Test-Path -LiteralPath $exe)) {
    throw "Virtual Device App compile failed (exit=$compileExit). See $(Join-Path $outDir 'compile.log')"
}

$scenario = @"
boot
seed
touch start:demo-math-001
touch pause:demo-math-001
touch resume:demo-math-001
touch complete:demo-math-001
restart
summary
quit
"@
$lines = @($scenario | & $exe 2>&1)
$runExit = $LASTEXITCODE
$lines | Set-Content -LiteralPath (Join-Path $outDir "scenario.jsonl") -Encoding utf8
if ($runExit -ne 0) { throw "Virtual Device App scenario failed (exit=$runExit)" }

$records = @($lines | Where-Object { $_ -is [string] -and $_.Trim() } | ForEach-Object { $_ | ConvertFrom-Json })
if ($records.Count -lt 7) { throw "Virtual Device App emitted too few records: $($records.Count)" }
$completed = $records | Where-Object { $_.op -eq "touch_complete" } | Select-Object -Last 1
$restarted = $records | Where-Object { $_.op -eq "restart" } | Select-Object -Last 1
$summary = $records | Where-Object { $_.op -eq "summary" } | Select-Object -Last 1
if (-not $completed.ok) { throw "Complete transition was not accepted" }
if (-not $restarted.ok) { throw "Restart did not recover" }
if ($summary.pending -ne 6) { throw "Expected 6 pending events after full loop, got $($summary.pending)" }
if ($null -ne $summary.active) { throw "Expected no active session after completion" }

$result = [ordered]@{
    task = "CODEX-APP-FIRST-001"
    checkpoint = "AF0"
    passed = $true
    compiler = $CompilerPath
    executable = $exe
    scenario = (Join-Path $outDir "scenario.jsonl")
    pendingAfterRestart = $summary.pending
    activeAfterRestart = $summary.active
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outDir "summary.json") -Encoding utf8
$result | ConvertTo-Json -Depth 5
