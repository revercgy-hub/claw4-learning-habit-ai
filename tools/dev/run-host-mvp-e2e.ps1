# claw4/tools/dev/run-host-mvp-e2e.ps1
# Host-MVP end-to-end orchestrator (WB-STREAM-002 CP7/CP8).
#
# Runs, in order:
#   1) native C++ host gate  (domain/outbox/coordinator/presenter + P4 iface)
#      -> proves offline Start/Pause/Resume/Complete produce pending events
#      that converge after reconnect (native, actually-executed tests)
#   2) backend pytest suite  (device contract + family backend + e2e loop +
#      schema-drift guard against the PWA's API client)
#   3) PWA typecheck + vitest + production build (same API schema as the
#      backend served to the e2e loop)
#
# Everything is 127.0.0.1 / in-process: no public listener is ever started,
# no external database/NAS/service is contacted. Temporary SQLite databases
# are cleaned by tests/conftest.py on each pytest session; no processes are
# left behind. A failing step propagates a non-zero exit code.
#
# Usage:
#   powershell -File tools/dev/run-host-mvp-e2e.ps1 `
#       -CompilerPath E:\...\w64devkit-2.9.1\bin\g++.exe `
#       [-CrossCompilerPath E:\...\riscv32-esp-elf-g++.exe] `
#       [-LogDir out\e2e]
param(
    [Parameter(Mandatory = $true)][string]$CompilerPath,
    [string]$CrossCompilerPath = "",
    [string]$LogDir = "out\e2e"
)

$ErrorActionPreference = "Continue"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$LogDirAbs = Join-Path $RepoRoot $LogDir
New-Item -ItemType Directory -Force -Path $LogDirAbs | Out-Null
$Log = Join-Path $LogDirAbs "e2e_result.txt"
Remove-Item $Log -ErrorAction SilentlyContinue

function Write-Log([string]$msg) {
    $line = "{0} {1}" -f (Get-Date -Format "HH:mm:ss"), $msg
    Write-Host $line
    Add-Content -Encoding utf8 $Log $line
}

$global:FAILED = $false

# --- locate helpers -------------------------------------------------------
$hostGate = Join-Path (Split-Path -Parent $PSScriptRoot) "dev\verify-host-cpp-tests.ps1"
if (-not (Test-Path $hostGate)) {
    # fall back: same tools\dev directory as this script
    $hostGate = Join-Path $PSScriptRoot "verify-host-cpp-tests.ps1"
}

# --- 1) native C++ host gate ---------------------------------------------
Write-Log "== [1/3] native C++ host gate =="
if (-not (Test-Path $hostGate)) {
    Write-Log "FAIL: verify-host-cpp-tests.ps1 not found ($hostGate)"
    $global:FAILED = $true
} else {
    # Pass the compiler paths as explicit named parameters (a plain array
    # splat would bind them positionally and break the parameters).
    if ($CrossCompilerPath) {
        & $hostGate -CompilerPath $CompilerPath -CrossCompilerPath $CrossCompilerPath *>> $Log
    } else {
        & $hostGate -CompilerPath $CompilerPath *>> $Log
    }
    if ($LASTEXITCODE -eq 0) {
        Write-Log "C++ host gate : PASS (exit=0; offline domain/outbox semantics proven)"
    } else {
        Write-Log "C++ host gate : FAIL (exit=$LASTEXITCODE)"
        $global:FAILED = $true
    }
}

# --- 2) backend pytest (contract + family + e2e + schema drift) ----------
Write-Log ""
Write-Log "== [2/3] backend pytest =="
$backendDir = Join-Path $RepoRoot "backend"
$py = Join-Path $backendDir ".venv\Scripts\python.exe"
if (-not (Test-Path $py)) {
    Write-Log "FAIL: backend venv missing ($py) - run backend dependency install first"
    $global:FAILED = $true
} else {
    Push-Location $backendDir
    & $py -m pytest tests/ 2>&1 | Out-File -Append -Encoding utf8 $Log
    $code = $LASTEXITCODE
    Pop-Location
    if ($code -eq 0) {
        Write-Log "backend pytest : PASS (exit=0)"
    } else {
        Write-Log "backend pytest : FAIL (exit=$code)"
        $global:FAILED = $true
    }
}

# --- 3) PWA typecheck + test + production build --------------------------
Write-Log ""
Write-Log "== [3/3] PWA typecheck + test + build =="
$frontendDir = Join-Path $RepoRoot "frontend"
$npm = "npm.cmd"
if (-not (Test-Path (Join-Path $frontendDir "package.json"))) {
    Write-Log "FAIL: frontend package.json missing"
    $global:FAILED = $true
} else {
    Push-Location $frontendDir
    foreach ($step in @("typecheck", "test", "build")) {
        & $npm run $step 2>&1 | Out-File -Append -Encoding utf8 $Log
        $code = $LASTEXITCODE
        if ($code -eq 0) {
            Write-Log "PWA $step : PASS"
        } else {
            Write-Log "PWA $step : FAIL (exit=$code)"
            $global:FAILED = $true
        }
    }
    Pop-Location
}

# --- summary --------------------------------------------------------------
Write-Log ""
if ($global:FAILED) {
    Write-Log "E2E RESULT: FAIL"
    exit 1
}
Write-Log "E2E RESULT: PASS (all steps on 127.0.0.1 / in-process; nothing left running)"
exit 0
