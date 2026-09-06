# CODEX-APP-FIRST-001 AF0 — unified App-first verification gate.
#
# Modes:
#   Quick   native C++ host gate + Virtual Device App (fast feedback for edits)
#   Full    native C++ + backend + PWA gate (batch checkpoint)
#   IdfBuild  Full gate plus an optional IDF build in an external build tree
#
# The script never flashes a device, starts a public listener, or changes
# partition/bootloader/OTA configuration. Generated logs and summaries belong
# under out/ and are ignored by the repository.

[CmdletBinding()]
param(
    [ValidateSet("Quick", "Full", "IdfBuild")]
    [string]$Mode = "Quick",
    [string]$CompilerPath = "",
    [string]$CrossCompilerPath = "",
    [string]$PythonPath = "",
    [string]$FrontendPath = "",
    [string]$IdfProjectPath = "E:\c",
    [string]$IdfBuildPath = "E:\b",
    [string]$LogDir = "out\app-first"
)

$ErrorActionPreference = "Stop"
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { $PWD.Path }
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$logDirAbs = if ([IO.Path]::IsPathRooted($LogDir)) { $LogDir } else { Join-Path $repoRoot $LogDir }
New-Item -ItemType Directory -Force -Path $logDirAbs | Out-Null
$summaryPath = Join-Path $logDirAbs "gate-summary.json"
$transcriptPath = Join-Path $logDirAbs "gate.log"

$results = [ordered]@{}
$startedAt = Get-Date

function Add-Result {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][bool]$Passed,
        [int]$ExitCode = 0,
        [string]$Detail = ""
    )
    $results[$Name] = [ordered]@{
        passed = $Passed
        exitCode = $ExitCode
        detail = $Detail
    }
}

function Resolve-Executable {
    param(
        [string]$Explicit,
        [string[]]$Names,
        [string[]]$Candidates
    )
    if ($Explicit) {
        if (Test-Path -LiteralPath $Explicit) { return (Resolve-Path -LiteralPath $Explicit).Path }
        throw "Explicit executable does not exist: $Explicit"
    }
    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return ""
}

function Invoke-GateScript {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [hashtable]$Arguments = @{}
    )
    $output = & $ScriptPath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($output) { $output | Out-File -FilePath $transcriptPath -Append -Encoding utf8 }
    return $exitCode
}

"CODEX-APP-FIRST-001 gate" | Set-Content -LiteralPath $transcriptPath -Encoding utf8
"repo=$repoRoot" | Out-File -FilePath $transcriptPath -Append -Encoding utf8
"mode=$Mode" | Out-File -FilePath $transcriptPath -Append -Encoding utf8
"started=$($startedAt.ToString('o'))" | Out-File -FilePath $transcriptPath -Append -Encoding utf8

try {
    $compilerCandidates = @(
        "E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe",
        "E:\workbuddy\toolchains\llvm\bin\clang++.exe",
        "C:\Program Files\LLVM\bin\clang++.exe"
    )
    $compiler = Resolve-Executable -Explicit $CompilerPath -Names @("g++.exe", "clang++.exe") -Candidates $compilerCandidates
    if (-not $compiler) {
        Add-Result -Name "compiler" -Passed $false -ExitCode 2 -Detail "No native C++17 compiler found"
        throw "No native C++17 compiler found. Pass -CompilerPath explicitly."
    }
    $compilerDir = Split-Path -Parent $compiler
    $env:PATH = "$compilerDir;$env:PATH"
    Add-Result -Name "compiler" -Passed $true -Detail $compiler

    $cross = ""
    if ($CrossCompilerPath) {
        $cross = Resolve-Executable -Explicit $CrossCompilerPath -Names @() -Candidates @()
    } else {
        $cross = Resolve-Executable -Explicit "" -Names @("riscv32-esp-elf-g++.exe") -Candidates @(
            "E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe",
            "E:\workbuddy\学习习惯培育AI\toolchains\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe"
        )
    }
    $crossArgs = @{}
    if ($cross) { $crossArgs.CrossCompilerPath = $cross }

    $hostScript = Join-Path $scriptDir "verify-host-cpp-tests.ps1"
    $hostArgs = @{ CompilerPath = $compiler; OutputDir = (Join-Path $logDirAbs "host-tests") } + $crossArgs
    $hostExit = Invoke-GateScript -ScriptPath $hostScript -Arguments $hostArgs
    Add-Result -Name "hostCpp" -Passed ($hostExit -eq 0) -ExitCode $hostExit -Detail "verify-host-cpp-tests.ps1"
    if ($hostExit -ne 0) { throw "Native C++ host gate failed with exit code $hostExit" }

    $virtualScript = Join-Path $scriptDir "verify-virtual-device-app.ps1"
    $virtualExit = Invoke-GateScript -ScriptPath $virtualScript -Arguments @{
        CompilerPath = $compiler
        OutputDir = (Join-Path $logDirAbs "virtual-device")
    }
    Add-Result -Name "virtualDeviceApp" -Passed ($virtualExit -eq 0) -ExitCode $virtualExit -Detail "verify-virtual-device-app.ps1"
    if ($virtualExit -ne 0) { throw "Virtual Device App gate failed with exit code $virtualExit" }

    if ($Mode -in @("Full", "IdfBuild")) {
        $e2eScript = Join-Path $scriptDir "run-host-mvp-e2e.ps1"
        $e2eArgs = @{ CompilerPath = $compiler; LogDir = (Join-Path $LogDir "host-e2e"); SkipHostCpp = $true }
        if ($cross) { $e2eArgs.CrossCompilerPath = $cross }
        if ($PythonPath) { $e2eArgs.PythonPath = $PythonPath }
        if ($FrontendPath) { $e2eArgs.FrontendPath = $FrontendPath }
        $e2eExit = Invoke-GateScript -ScriptPath $e2eScript -Arguments $e2eArgs
        Add-Result -Name "hostMvpE2e" -Passed ($e2eExit -eq 0) -ExitCode $e2eExit -Detail "run-host-mvp-e2e.ps1"
        if ($e2eExit -ne 0) { throw "Host MVP E2E gate failed with exit code $e2eExit" }
        $connectedPython = if ($PythonPath) { $PythonPath } else { Join-Path $repoRoot "backend\.venv\Scripts\python.exe" }
        $connectedOutput = & $connectedPython (Join-Path $scriptDir "verify-connected-learning-app.py") --repo $repoRoot --compiler $compiler --out-dir (Join-Path $logDirAbs "connected-app") 2>&1
        $connectedExit = $LASTEXITCODE
        if ($connectedOutput) { $connectedOutput | Out-File -FilePath $transcriptPath -Append -Encoding utf8 }
        Add-Result -Name "connectedLearningApp" -Passed ($connectedExit -eq 0) -ExitCode $connectedExit -Detail "5 HTTP rounds / 55 process restarts; browser/device are separate gates"
        if ($connectedExit -ne 0) { throw "Connected LearningApp gate failed with exit code $connectedExit" }
    }

    if ($Mode -eq "IdfBuild") {
        $idf = Get-Command idf.py -ErrorAction SilentlyContinue
        if (-not $idf) {
            Add-Result -Name "idfBuild" -Passed $false -ExitCode 2 -Detail "idf.py not found on PATH"
            throw "IdfBuild requested but idf.py is not on PATH."
        }
        if (-not (Test-Path -LiteralPath $IdfProjectPath)) {
            Add-Result -Name "idfBuild" -Passed $false -ExitCode 2 -Detail "IDF project path missing: $IdfProjectPath"
            throw "IDF project path does not exist: $IdfProjectPath"
        }
        New-Item -ItemType Directory -Force -Path $IdfBuildPath | Out-Null
        $idfOutput = & $idf.Source -C $IdfProjectPath -B $IdfBuildPath build 2>&1
        $idfExit = $LASTEXITCODE
        if ($idfOutput) { $idfOutput | Out-File -FilePath $transcriptPath -Append -Encoding utf8 }
        Add-Result -Name "idfBuild" -Passed ($idfExit -eq 0) -ExitCode $idfExit -Detail "idf.py -C $IdfProjectPath -B $IdfBuildPath build"
        if ($idfExit -ne 0) { throw "IDF build failed with exit code $idfExit" }
    }
}
catch {
    Add-Result -Name "overall" -Passed $false -ExitCode 1 -Detail $_.Exception.Message
}

$passed = $true
foreach ($entry in $results.Values) {
    if (-not $entry.passed) { $passed = $false }
}
if ($results.Count -eq 0) { $passed = $false }
$finishedAt = Get-Date
$summary = [ordered]@{
    task = "CODEX-APP-FIRST-001"
    mode = $Mode
    repo = $repoRoot
    commit = (& git -c "safe.directory=$repoRoot" -C $repoRoot rev-parse HEAD 2>$null)
    started = $startedAt.ToString("o")
    finished = $finishedAt.ToString("o")
    passed = $passed
    results = $results
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Output ($summary | ConvertTo-Json -Depth 8)
if (-not $passed) { exit 1 }
exit 0
