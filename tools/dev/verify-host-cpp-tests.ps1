# verify-host-cpp-tests.ps1
# WB-STREAM-002 CP0 — native Windows C++17 "compile, link, run" test gate.
#
# 1) Resolves a host C++17 compiler (explicit -CompilerPath, or auto-probe of
#    PATH / standard install locations). Never hard-codes the current user's
#    path as the ONLY way to run.
# 2) Compiles firmware/tests/host/smoke_test.cpp (C++17, warnings-as-errors)
#    to out/host-tests/, links it, then actually RUNS it and propagates the
#    exit code. Reports compile/link/run separately.
# 3) Continues by invoking verify-interface-contracts.ps1 so the P4 cross
#    compile interface checks are proven not to regress.
#
# Usage:
#   .\verify-host-cpp-tests.ps1 -CompilerPath "C:\...\clang++.exe"
#   .\verify-host-cpp-tests.ps1                       # auto-probe
#   .\verify-host-cpp-tests.ps1 -CompilerPath ... -CrossCompilerPath "...\riscv32-esp-elf-g++.exe"
param(
    [string]$CompilerPath = "",
    [string]$CrossCompilerPath = ""
)

$ErrorActionPreference = "Continue"  # native stderr becomes ErrorRecords under "Stop" (PS 5.1)

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { $PWD.Path }
$RepoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$SmokeSrc = Join-Path $RepoRoot "firmware\tests\host\smoke_test.cpp"
$OutDir = Join-Path $RepoRoot "out\host-tests"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$Log = Join-Path $OutDir "host_result.txt"
Remove-Item $Log -ErrorAction SilentlyContinue
function Write-Log([string]$m) { $m | Out-File -Append -Encoding utf8 $Log }
$global:FAILED = $false

Write-Log "== WB-STREAM-002 CP0: native C++17 test gate =="
Write-Log "Repo   : $RepoRoot"
Write-Log "Smoke  : $SmokeSrc"
Write-Log "OutDir : $OutDir"
Write-Log "Time   : $(Get-Date -Format o)"

# ---- 1) resolve host compiler ----
function Resolve-HostCompiler {
    param([string]$Explicit)
    if ($Explicit -and (Test-Path $Explicit)) { return $Explicit }
    $candidates = @()
    if ($Explicit) { $candidates += $Explicit }
    # PATH
    foreach ($name in @("clang++.exe", "g++.exe", "cl.exe")) {
        $p = Get-Command $name -ErrorAction SilentlyContinue
        if ($p) { $candidates += $p.Source }
    }
    # standard install locations (user-scope winget first, then Program Files)
    $roots = @(
        (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"),
        "C:\Program Files\LLVM\bin",
        "C:\Program Files\Microsoft Visual Studio"
    )
    foreach ($root in $roots) {
        if (-not $root -or -not (Test-Path $root)) { continue }
        $hits = Get-ChildItem -Path $root -Filter clang++.exe -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 1
        if ($hits) { $candidates += $hits.FullName }
    }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return $c }
    }
    return ""
}

$cc = Resolve-HostCompiler $CompilerPath
Write-Log ""
Write-Log "== 1) compiler resolution =="
if (-not $cc) {
    Write-Log "FAIL: no usable host C++17 compiler found. Install e.g. LLVM via:"
    Write-Log '      winget install --id LLVM.LLVM -e --scope user'
    Write-Log "BLOCKED: cannot run host tests without a native compiler."
    exit 2
}
# MinGW-w64 (e.g. w64devkit) gcc locates `as`/`ld` through PATH, so the
# compiler's own bin directory must be visible to child processes.
$compilerDir = Split-Path -Parent $cc
$env:PATH = "$compilerDir;$env:PATH"
$verLine = (& $cc --version 2>&1 | Select-Object -First 1)
Write-Log "Compiler : $cc"
Write-Log "Version  : $verLine"

# ---- 2) compile ----
Write-Log ""
Write-Log "== 2) compile (C++17, warnings-as-errors) =="
$obj = Join-Path $OutDir "smoke_test.obj"
Remove-Item $obj -ErrorAction SilentlyContinue
$cout = & $cc -std=c++17 -Wall -Wextra -Werror -I (Join-Path $RepoRoot "firmware\main") `
               -c $SmokeSrc -o $obj 2>&1
$ccode = $LASTEXITCODE
if ($cout) { $cout | Out-File -Append -Encoding utf8 $Log }
if ($ccode -eq 0 -and (Test-Path $obj)) {
    Write-Log "compile : PASS (obj=$obj)"
} else {
    Write-Log "compile : FAIL (exit=$ccode)"
    $global:FAILED = $true
}

# ---- 3) link ----
Write-Log ""
Write-Log "== 3) link =="
$exe = Join-Path $OutDir "smoke_test.exe"
Remove-Item $exe -ErrorAction SilentlyContinue
$lout = & $cc $obj -o $exe 2>&1
$lcode = $LASTEXITCODE
if ($lout) { $lout | Out-File -Append -Encoding utf8 $Log }
if ($lcode -eq 0 -and (Test-Path $exe)) {
    Write-Log "link    : PASS (exe=$exe)"
} else {
    Write-Log "link    : FAIL (exit=$lcode)"
    $global:FAILED = $true
}

# ---- 4) run ----
Write-Log ""
Write-Log "== 4) run =="
if ($global:FAILED) {
    Write-Log "run     : SKIPPED (compile/link failed)"
} else {
    $rout = & $exe 2>&1
    $rcode = $LASTEXITCODE
    if ($rout) { $rout | Out-File -Append -Encoding utf8 $Log }
    if ($rcode -eq 0) {
        Write-Log "run     : PASS (exit=0)"
    } else {
        Write-Log "run     : FAIL (exit=$rcode)"
        $global:FAILED = $true
    }
}

# ---- 5) interface cross-compile contract (no regression) ----
Write-Log ""
Write-Log "== 5) verify-interface-contracts.ps1 (P4 cross compile) =="
$ifaceScript = Join-Path $scriptDir "verify-interface-contracts.ps1"
if (Test-Path $ifaceScript) {
    $cross = $CrossCompilerPath
    if (-not $cross) {
        # auto-probe the known claw4-idf-tools layout (not hard-coded as the only way)
        $probe = Join-Path "E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe"
        if (Test-Path $probe) { $cross = $probe }
    }
    if ($cross) {
        & $ifaceScript -CompilerPath $cross *>> $Log
        Write-Log "interface: exit=$LASTEXITCODE (see above; 0 == PASS)"
        if ($LASTEXITCODE -ne 0) { $global:FAILED = $true }
    } else {
        # fall back to PATH resolution inside the interface script
        & $ifaceScript *>> $Log
        Write-Log "interface: exit=$LASTEXITCODE (PATH-resolved cross compiler)"
        if ($LASTEXITCODE -ne 0) { $global:FAILED = $true }
    }
} else {
    Write-Log "interface: SKIPPED (verify-interface-contracts.ps1 not found)"
}

Write-Log ""
if ($global:FAILED) {
    Write-Log "RESULT: FAIL"
    exit 1
} else {
    Write-Log "RESULT: NATIVE CPP TEST GATE PASS"
    exit 0
}
