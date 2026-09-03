# verify-interface-contracts.ps1
# WB-STREAM-001 CP1 — compile-time interface contract verification for the
# portable C++17 interface skeleton under firmware/.
#
# Runs `-fsyntax-only` with a cross compiler over every interface header and
# the compile-time contract tests, and scans for forbidden hardware
# dependencies in learning_domain. Every step is recorded to
# <repo>/out/verify-interface-contracts/verify_result.txt (git-ignored).
#
# Usage:
#   .\verify-interface-contracts.ps1 -CompilerPath "E:\...\riscv32-esp-elf-g++.exe"
#   .\verify-interface-contracts.ps1                        # uses `riscv32-esp-elf-g++` from PATH if available
#
# The compiler path is intentionally NOT hard-coded as the only way to run;
# pass -CompilerPath with any C++17 compiler (e.g. the ESP32-P4 RISC-V
# toolchain from claw4-idf-tools). Output goes to <repo>/out/ which is
# git-ignored.
param(
    [string]$CompilerPath = "riscv32-esp-elf-g++"
)

# NOTE: keep "Continue". With "Stop", PowerShell 5.1 wraps native stderr
# (e.g. the harmless "pragma once in main file" warning when compiling a
# header directly) into ErrorRecords and aborts the loop. Correctness is
# enforced via $global:FAILED and exit codes instead.
$ErrorActionPreference = "Continue"

# $PSScriptRoot is empty when invoked through -Command; fall back to CWD.
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { $PWD.Path }
$RepoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$MainDir = Join-Path $RepoRoot "firmware\main"
$TestsDir = Join-Path $RepoRoot "firmware\tests\contracts"
$OutDir = Join-Path $RepoRoot "out\verify-interface-contracts"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$Log = Join-Path $OutDir "verify_result.txt"
$HeaderErr = Join-Path $OutDir "header_errors.txt"
$ContractErr = Join-Path $OutDir "contract_errors.txt"
Remove-Item $Log, $HeaderErr, $ContractErr -ErrorAction SilentlyContinue

function Write-Log([string]$m) { $m | Out-File -Append -Encoding utf8 $Log }

Write-Log "== WB-STREAM-001 CP1: interface contract verification =="
Write-Log "Compiler : $CompilerPath"
Write-Log "MainDir  : $MainDir"
Write-Log "OutDir   : $OutDir"
Write-Log "Time     : $(Get-Date -Format o)"
$global:FAILED = $false

# 1) Dependency scan: learning_domain must not include hardware/OS headers.
Write-Log ""
Write-Log "== 1) learning_domain dependency scan =="
$Forbidden = @("lvgl", "esp_", "freertos", "driver/", "bsp", "wifi", "nvs", "hal")
$domainFiles = Get-ChildItem -Path (Join-Path $MainDir "learning_domain") -Filter *.h -Recurse
$violations = @()
foreach ($f in $domainFiles) {
    $content = Get-Content -Raw $f.FullName
    foreach ($inc in [regex]::Matches($content, '#\s*include\s*[<"]([^>"]+)')) {
        $header = $inc.Groups[1].Value.ToLowerInvariant()
        foreach ($tok in $Forbidden) {
            if ($header -like "*$tok*") {
                $violations += "$($f.Name): forbidden include <$($inc.Groups[1].Value)>"
            }
        }
    }
}
if ($violations.Count -eq 0) {
    Write-Log "PASS: no forbidden hardware/OS includes in learning_domain ($($domainFiles.Count) headers)"
} else {
    Write-Log "FAIL: forbidden includes found:"
    $violations | ForEach-Object { Write-Log "  $_" }
    $global:FAILED = $true
}

# 2) Compile-time syntax check of every interface header.
Write-Log ""
Write-Log "== 2) -fsyntax-only over all interface headers =="
$headers = Get-ChildItem -Path $MainDir -Filter *.h -Recurse
$ok = 0
foreach ($h in $headers) {
    $out = & $CompilerPath -std=c++17 -fsyntax-only -Wall -Wextra -I $MainDir $h.FullName 2>&1
    $code = $LASTEXITCODE
    if ($out) { $out | Out-File -Append -Encoding utf8 $HeaderErr }
    if ($code -eq 0) { $ok++ } else { $global:FAILED = $true; Write-Log "FAIL header: $($h.FullName)" }
}
Write-Log "headers  : $ok / $($headers.Count) PASS"

# 2.5) Host implementation sources under interaction/mcp; 3) contract tests.
Write-Log ""
Write-Log "== 2.5) -fsyntax-only over host implementation sources (interaction/mcp) =="
$hostCppDirs = @("interaction", "mcp")
$cppOk = 0
$cppTotal = 0
foreach ($sub in $hostCppDirs) {
    $dir = Join-Path $MainDir $sub
    if (-not (Test-Path $dir)) { continue }
    $cppFiles = Get-ChildItem -Path $dir -Filter *.cpp -Recurse
    foreach ($cpp in $cppFiles) {
        $cppTotal++
        $outCpp = & $CompilerPath -std=c++17 -fsyntax-only -Wall -Wextra -I $MainDir $cpp.FullName 2>&1
        $codeCpp = $LASTEXITCODE
        if ($outCpp) { $outCpp | Out-File -Append -Encoding utf8 $ContractErr }
        if ($codeCpp -eq 0) { $cppOk++ } else {
            $global:FAILED = $true
            Write-Log "FAIL impl source: $($cpp.FullName)"
        }
    }
}
Write-Log "implsrcs : $cppOk / $cppTotal PASS (interaction/mcp on target ISA)"

Write-Log ""
Write-Log "== 3) -fsyntax-only over contract tests =="
$testCpp = Join-Path $TestsDir "contract_tests.cpp"
$out2 = & $CompilerPath -std=c++17 -fsyntax-only -Wall -Wextra -I $MainDir $testCpp 2>&1
$code2 = $LASTEXITCODE
if ($out2) { $out2 | Out-File -Append -Encoding utf8 $ContractErr }
if ($code2 -eq 0) {
    Write-Log "contract : 1/1 PASS (contract_tests.cpp)"
} else {
    $global:FAILED = $true
    Write-Log "contract : FAIL (see $ContractErr)"
}

Write-Log ""
if ($global:FAILED) {
    Write-Log "RESULT: FAIL"
} else {
    Write-Log "RESULT: ALL INTERFACE CONTRACT CHECKS PASS"
}
Write-Log "Full log: $Log"
if ($global:FAILED) { exit 1 } else { exit 0 }
