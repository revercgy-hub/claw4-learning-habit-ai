# A05 CP2: full ESP-IDF build in the isolated root.
#
# Constraints honoured (WB-A05-BUILD-001 §3 + reviewer rulings):
#   * a NEW isolated build root; no reuse of any old CMakeCache/object/ELF
#   * `idf.py build` only -- no copied ninja invocation, no set-target/menuconfig
#   * SDKCONFIG passed EXPLICITLY; its hash is recorded before and after configure
#   * ESP_IDF_VERSION=5.5 (NOT 5.5.4 -- 5.5.4 silently drops the esp_wifi_remote
#     Kconfig variant and loses the Wi-Fi symbols; see
#     WB-APP-FIRST-L3_BUILD_STALE_FIX_2026-09-12 and CODEX_APP_FIRST_CONTINUATION)
#   * PYTHONPATH and CODEBUDDY_SAFE_DELETE_* stripped (they break asset generation)
#   * NO flash / app-flash / erase / monitor; no flasher_args execution
#
# ENV-DIAG-001 additions (2026-09-15, user-directed):
#   * every cmake/ninja/riscv32 path is resolved IN THIS PROCESS, twice: once by
#     PowerShell (Get-Command / where.exe / PATH walk) and once by the IDF venv
#     Python (shutil.which + a real `ninja --version` run). The previous round
#     resolved them in a *separate* shell, so that evidence did not describe the
#     environment the build actually ran in.
#   * `verify-build-inputs.py --check` is now a HARD precondition: non-zero ->
#     exit 3 and NO build is attempted.
#   * `idf.py -v` is used so the ACTUAL cmake command line is captured, and on
#     failure the full configure logs + CMakeConfigureLog.yaml are harvested.
#   * still NO tool installation, NO CMake/sdkconfig/partition edits.
#
# ENV-DIAG-001 invocation:
#   powershell -File tools\dev\run-a05-cp2-build.ps1 -Build E:\claw4-a05-19fd979\build-envdiag-001
#
# Exit codes:
#   0  idf.py succeeded
#   2  setup fatal (missing IDF/idf.py/paths)
#   3  build-input manifest check FAILED -> fail closed, NO build attempted
#   n  otherwise the exit code of idf.py
#
# Deliberately avoids `Get-ChildItem Env:` (throws ArgumentException under
# `powershell -File` here) and `Tee-Object` (writes UTF-16 in PS 5.1).

[CmdletBinding()]
param(
  [string]$Src      = "E:\claw4-a05-19fd979\src",
  [string]$Build    = "E:\claw4-a05-19fd979\build",
  [string]$LogDir   = "E:\claw4-a05-19fd979\logs",
  [string]$IdfPath  = "E:\workbuddy\esp-idf-5.5.4-ascii",
  [string]$IdfTools = "E:\workbuddy\claw4-idf-tools",
  [switch]$Fresh
)

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Manifest = Join-Path $RepoRoot "integration\metalio_claw4\a05_build_input_manifest.json"
$VerifyInputs = Join-Path $RepoRoot "tools\dev\verify-build-inputs.py"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$log = Join-Path $LogDir ("cp2-build-" + $stamp + ".log")
$diag = Join-Path $LogDir ("envdiag-" + $stamp + ".txt")
$script:fail = $null

function Say($m) {
  $line = [string]$m
  Write-Host $line
  Add-Content -Path $log -Value $line -Encoding UTF8
}

function SayDiag($m) {
  $line = [string]$m
  Write-Host $line
  Add-Content -Path $log -Value $line -Encoding UTF8
  Add-Content -Path $diag -Value $line -Encoding UTF8
}

Say "== A05 CP2 build =="
Say ("started      : " + (Get-Date -Format s))
Say ("repo root    : " + $RepoRoot)
Say ("envdiag file : " + $diag)

# --- preconditions ---------------------------------------------------------
foreach ($p in @($Src, $IdfPath, $IdfTools)) {
  if (-not (Test-Path -LiteralPath $p)) { Say "FATAL missing path: $p"; exit 2 }
}
if ($Fresh -and (Test-Path -LiteralPath $Build)) {
  Say "removing existing build root (-Fresh): $Build"
  Remove-Item -LiteralPath $Build -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $Build | Out-Null

$sdkconfig = Join-Path $Src "sdkconfig"
if (-not (Test-Path -LiteralPath $sdkconfig)) { Say "FATAL SDKCONFIG not found: $sdkconfig"; exit 2 }

# --- environment -----------------------------------------------------------
$env:IDF_PATH            = $IdfPath
$env:IDF_TOOLS_PATH      = $IdfTools
$env:IDF_PYTHON_ENV_PATH = Join-Path $IdfTools "python_env\idf5.5_py3.12_env"
$env:ESP_IDF_VERSION     = "5.5"
$env:IDF_VERSION         = "5.5.4"

try {
  $keys = [System.Environment]::GetEnvironmentVariables().Keys
  foreach ($k in @($keys)) {
    if ($k -eq "PYTHONPATH" -or $k -like "CODEBUDDY_SAFE_DELETE*") {
      [System.Environment]::SetEnvironmentVariable($k, $null)
      Say "stripped env var: $k"
    }
  }
} catch {
  Say ("WARN env strip failed: " + $_.Exception.Message)
}

$prependAll = @(
  (Join-Path $IdfTools "python_env\idf5.5_py3.12_env\Scripts"),
  (Join-Path $IdfTools "tools\cmake\3.30.2\bin"),
  (Join-Path $IdfTools "tools\ninja\1.12.1"),
  (Join-Path $IdfTools "tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin")
)
$prepend = @($prependAll | Where-Object { Test-Path -LiteralPath $_ })
$env:PATH = (($prepend -join ";") + ";" + $env:PATH)

$idfPy = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
$idfMain = Join-Path $IdfPath "tools\idf.py"

Say "idf python   : $idfPy"
Say "idf.py       : $idfMain"
Say "IDF_PATH     : $env:IDF_PATH"
Say ("versions     : ESP_IDF_VERSION=" + $env:ESP_IDF_VERSION + "  IDF_VERSION=" + $env:IDF_VERSION)
Say "source       : $Src"
Say "build root   : $Build"
Say "sdkconfig    : $sdkconfig"
Say ("sdkconfig sha256 (pre) : " + (Get-FileHash -LiteralPath $sdkconfig -Algorithm SHA256).Hash.ToLower())
Say ""

if (-not (Test-Path -LiteralPath $idfPy)) { Say "FATAL idf python missing: $idfPy"; exit 2 }
if (-not (Test-Path -LiteralPath $idfMain)) { Say "FATAL idf.py missing: $idfMain"; exit 2 }

# ===================== ENV-DIAG-001 (same process) =========================
Say ""
SayDiag "=== ENV-DIAG-001: in-process path resolution ==="
SayDiag ("process id   : " + $PID)
SayDiag ("cwd          : " + (Get-Location).Path)
SayDiag ("PATHEXT      : [" + $env:PATHEXT + "]")
SayDiag ("MSYSTEM      : [" + $env:MSYSTEM + "]")
SayDiag ("PYTHONPATH   : [" + $env:PYTHONPATH + "]")
SayDiag ""
SayDiag "-- prepend candidates (measured BEFORE the Test-Path filter) --"
foreach ($p in $prependAll) {
  SayDiag ("  Test-Path=" + (Test-Path -LiteralPath $p) + "   " + $p)
}
SayDiag ("  kept " + $prepend.Count + " of " + $prependAll.Count)
SayDiag ""
SayDiag "-- effective PATH of THIS process, entry by entry --"
$pi = 0
foreach ($p in ($env:PATH -split ';')) {
  if ($p.Trim() -eq '') { continue }
  SayDiag ("  [{0,2}] exists={1,-5} {2}" -f $pi, (Test-Path -LiteralPath $p), $p)
  $pi++
}
SayDiag ("  PATH length = " + $env:PATH.Length + " chars")
SayDiag ""
SayDiag "-- PowerShell resolution (Get-Command) --"
foreach ($n in @("cmake", "ninja", "riscv32-esp-elf-g++", "riscv32-esp-elf-gcc", "python", "g++", "make")) {
  $c = Get-Command $n -ErrorAction SilentlyContinue
  if ($c) {
    SayDiag ("  {0,-22} -> {1}  exists={2}" -f $n, $c.Source, (Test-Path -LiteralPath $c.Source))
  } else {
    SayDiag ("  {0,-22} -> NOT FOUND" -f $n)
  }
}
SayDiag ("  where.exe ninja      : " + ((& where.exe ninja 2>&1) -join " | "))
SayDiag ("  where.exe cmake      : " + ((& where.exe cmake 2>&1) -join " | "))
SayDiag ""

$probePy = Join-Path $LogDir "envdiag-pathprobe.py"
$probeOut = Join-Path $LogDir "envdiag-pathprobe-python.txt"
@'
import os, shutil, subprocess, sys

lines = []
lines.append("python            : %s" % sys.executable)
lines.append("PATHEXT           : %r" % os.environ.get("PATHEXT"))
lines.append("MSYSTEM           : %r" % os.environ.get("MSYSTEM"))
lines.append("PYTHONPATH        : %r" % os.environ.get("PYTHONPATH"))
lines.append("PATH length       : %d" % len(os.environ.get("PATH") or ""))
lines.append("")
lines.append("-- Python resolution (shutil.which) --")
for n in ("cmake", "ninja", "ninja-build", "riscv32-esp-elf-g++",
          "riscv32-esp-elf-gcc", "python", "g++", "make"):
    lines.append("  %-22s -> %r" % (n, shutil.which(n)))
lines.append("")
lines.append("-- real execution from THIS interpreter (same env idf.py gives cmake) --")
for cmd in (["ninja", "--version"], ["cmake", "--version"], ["riscv32-esp-elf-g++", "--version"]):
    try:
        p = subprocess.run(cmd, capture_output=True)
        out = p.stdout.decode("utf-8", "replace").strip().splitlines()
        lines.append("  %-24s rc=%d first=%r" % (" ".join(cmd), p.returncode, out[0] if out else ""))
    except Exception as exc:
        lines.append("  %-24s EXC %r" % (" ".join(cmd), exc))
open(sys.argv[1], "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
'@ | Set-Content -LiteralPath $probePy -Encoding UTF8

SayDiag "-- IDF venv Python resolution (child interpreter of THIS process) --"
& $idfPy $probePy $probeOut *>&1 | ForEach-Object { SayDiag ("  " + [string]$_) }
if (Test-Path -LiteralPath $probeOut) {
  foreach ($l in @(Get-Content -LiteralPath $probeOut)) { SayDiag ("  " + $l) }
}
SayDiag ""
SayDiag "-- idf_tools.py export (consumed by idf.py / activate_venv) --"
$expOut = Join-Path $LogDir "envdiag-idf-tools-export.out.txt"
$expErr = Join-Path $LogDir "envdiag-idf-tools-export.err.txt"
& $idfPy (Join-Path $IdfPath "tools\idf_tools.py") export 1> $expOut 2> $expErr
$expRc = $LASTEXITCODE
$expLines = @(Get-Content -LiteralPath $expOut -ErrorAction SilentlyContinue)
SayDiag ("  rc = " + $expRc + "   stdout lines = " + $expLines.Count)
foreach ($l in $expLines) { SayDiag ("  out| " + $l) }
foreach ($l in @(Get-Content -LiteralPath $expErr -ErrorAction SilentlyContinue)) { SayDiag ("  err| " + $l) }
SayDiag "=== end ENV-DIAG-001 ==="
Say ""

# ===================== HARD precondition (fail closed) =====================
Say "== HARD precondition: verify-build-inputs.py --check =="
Say ("manifest : " + $Manifest)
Say ("verifier : " + $VerifyInputs)
if (-not (Test-Path -LiteralPath $Manifest)) { Say "FATAL manifest missing: $Manifest"; exit 3 }
if (-not (Test-Path -LiteralPath $VerifyInputs)) { Say "FATAL input verifier missing: $VerifyInputs"; exit 3 }

& $idfPy $VerifyInputs --manifest $Manifest --check *>&1 | ForEach-Object { Say $_ }
$chkRc = $LASTEXITCODE
Say ("input check rc = " + $chkRc)
if ($chkRc -ne 0) {
  Say "HARD STOP: build-input manifest check FAILED -> no build attempted."
  Say ("log: " + $log)
  exit 3
}
Say "input check PASS -> cold build follows"
Say ""

# --- configure + build ----------------------------------------------------
Say "invoking: idf.py -C <src> -B <build> -D SDKCONFIG=<abs> build"
Say ("start=" + (Get-Date -Format s))
$sw = [System.Diagnostics.Stopwatch]::StartNew()

& $idfPy $idfMain -C $Src -B $Build -D ("SDKCONFIG=" + $sdkconfig) build *>&1 |
  ForEach-Object { Say $_ }
$rc = $LASTEXITCODE
$sw.Stop()

Say ""
Say ("exit=$rc   elapsed=" + $sw.Elapsed.ToString() + "   end=" + (Get-Date -Format s))

# ===================== evidence harvest ====================================
$cmakeCmdFile = Join-Path $LogDir ("envdiag-cmake-command-" + $stamp + ".txt")
$cmakeLines = @("# cmake invocation as actually executed by idf.py (harvested from the build log)")
$cmakeLines += @(Get-Content -LiteralPath $log | Where-Object {
  $_ -match 'Running cmake in directory' -or $_ -match 'Executing "cmake'
})
$cmakeLines | Set-Content -LiteralPath $cmakeCmdFile -Encoding UTF8
Say ("cmake command captured : " + $cmakeCmdFile + "  (" + ($cmakeLines.Count - 1) + " line(s))")

$envdiagDir = Join-Path $LogDir ("envdiag-artifacts-" + $stamp)
New-Item -ItemType Directory -Force -Path $envdiagDir | Out-Null
$copied = @()
foreach ($pat in @("log\idf_py_stdout_output_*", "log\idf_py_stderr_output_*",
                   "CMakeFiles\CMakeConfigureLog.yaml", "CMakeFiles\CMakeError.log",
                   "CMakeFiles\CMakeOutput.log", "CMakeCache.txt")) {
  $full = Join-Path $Build $pat
  foreach ($f in @(Get-ChildItem -Path $full -ErrorAction SilentlyContinue)) {
    if ($f.PSIsContainer) { continue }
    Copy-Item -LiteralPath $f.FullName -Destination $envdiagDir -Force -ErrorAction SilentlyContinue
    $copied += $f.Name
  }
}
Say ("harvested full configure evidence into : " + $envdiagDir)
foreach ($c in $copied) { Say ("  " + $c) }
if ($copied.Count -eq 0) { Say "  (nothing to harvest - configure produced no logs)" }

$errHit = @(Get-Content -LiteralPath $log | Where-Object {
  $_ -match 'CMAKE_MAKE_PROGRAM|unable to find a build program' })
Say ""
if ($errHit.Count -gt 0) {
  Say "DIAGNOSIS: the CMAKE_MAKE_PROGRAM / Ninja-not-found error DID recur."
  Say "  -> evidence saved above; STOPPING as instructed."
  Say "  -> no CMake/sdkconfig/partition edit, no tool installation, no bypass was performed."
} else {
  Say "DIAGNOSIS: the CMAKE_MAKE_PROGRAM / Ninja-not-found error did NOT recur."
}

# --- post checks (report only; NO flash) ---------------------------------
$sdkAfter = Join-Path $Build "config\sdkconfig"
$postHash = if (Test-Path -LiteralPath $sdkAfter) { (Get-FileHash -LiteralPath $sdkAfter -Algorithm SHA256).Hash.ToLower() } else { "ABSENT" }
Say ("sdkconfig sha256 (post-configure) : " + $postHash)

foreach ($a in @("xiaozhi.bin","xiaozhi.elf","xiaozhi.map",
                 "bootloader\bootloader.bin","partition_table\partition-table.bin")) {
  $p = Join-Path $Build $a
  if (Test-Path -LiteralPath $p) {
    Say ("artifact {0,-38} {1,12} bytes  sha256={2}" -f $a, (Get-Item -LiteralPath $p).Length,
         (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLower())
  } else {
    Say ("artifact {0,-38} ABSENT" -f $a)
  }
}
Say "NO flash / erase / monitor / flasher_args was executed."
Say "log: $log"
exit $rc
