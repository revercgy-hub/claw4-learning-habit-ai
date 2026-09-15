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
# Authorised remediation run (Codex CP2 review 2026-09-15 §8, option 1):
#   run this from an ORDINARY host terminal that is NOT under the WorkBuddy
#   shim/sandbox environment injection, e.g.
#     powershell -File tools\dev\run-a05-cp2-build.ps1 -Build E:\claw4-a05-19fd979\build-cp2-002 -Fresh
#   The P0 guard refuses to build while `os.environ['PATH']` still disagrees with
#   the real Win32 process PATH, so a wasted build attempt is impossible.
#   Still forbidden and still NOT used here: editing CMake / sdkconfig /
#   partition, -D CMAKE_MAKE_PROGRAM, ninja warm build, installing unrelated
#   Xtensa/ULP/DFU tools.
#
# CP2-RUNNER-FIX-001 (three review points, nothing else):
#   1. the SDKCONFIG "post" hash is a RE-HASH of the file actually passed via
#      -D SDKCONFIG=<abs>, taken after configure -- no longer <build>\config\
#      sdkconfig, which is a different, build-generated file.
#   2. ALL FIVE required artifacts are mandatory: if the build exits 0 but any
#      of xiaozhi.bin / xiaozhi.elf / xiaozhi.map / bootloader\bootloader.bin /
#      partition_table\partition-table.bin is missing -> exit 5.
#   3. the approved partition CSV now defaults to <src>\partitions\v1\
#      32m_dual.csv (inside the isolated tree); no default E:\c reference. The
#      file is checked up front (exit 3 if absent) and its raw AND LF-normalised
#      sha256 are logged, because the in-tree copy is LF while the E:\c mirror is
#      CRLF -- the two differ only in line endings, same 13-row layout.
#
# CP2-SOURCE-FIX-001 follow-up (harness gap found 2026-09-15 16:52):
#   The CP2 run at 16:52 passed verify-build-inputs 5/5 yet failed again with the
#   SAME learning_screen.cc:497 error, because -Src still defaulted to the
#   un-fixed tree E:\claw4-a05-19fd979\src while the re-frozen manifest now
#   describes E:\claw4-a05-19fd979\src-cp2-003. verify-build-inputs cannot catch
#   this: it validates the paths written INSIDE the manifest, not the tree the
#   runner builds from. Two changes:
#     a) the -Src default now points at the current authoritative replay tree;
#     b) a fail-closed binding check (below) requires -Src == manifest
#        isolated_src.origin, so the two can never silently diverge again.
#
# CP2-SOURCE-FIX-001 follow-up A (2026-09-15): SHORT SOURCE PATH IS MANDATORY.
#   The 17:09 run built the correct tree but died in ninja with
#     CreateProcess: The parameter is incorrect. (is the command line too long?)
#   because the Windows CreateProcess command line is capped at 32,767 chars and
#   the long TUs here carry 379 -I flags, 212 of which embed the source-root
#   path. Measured over the real compile_commands.json (2,422 entries):
#     tree .../src        -> longest cmd 31,759  (headroom 1,008, 0 over limit)
#     tree .../src-cp2-003-> longest cmd 33,463  (OVER by 696, 195 over limit)
#   So the tree was replayed into a SHORT root and the defaults are now
#     -Src   E:\a05c\s        -Build  E:\a05c\b
#   which projects to a 28,512-char longest command (headroom 4,255, 0 over).
#   DO NOT rename these directories to anything long, and do NOT try to "fix"
#   this by passing -D CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES (that is a CMake
#   configuration change and is forbidden by the task book).
#
# Exit codes:
#   0  idf.py succeeded AND every post-build verification passed
#   2  setup fatal (missing IDF/idf.py/paths)
#   3  build-input manifest check or source binding FAILED -> fail closed, NO build
#   4  host PATH desynchronisation still present -> fail closed, NO build attempted
#   5  build succeeded but a post-build verification FAILED
#   n  otherwise the exit code of idf.py
#
# Deliberately avoids `Get-ChildItem Env:` (throws ArgumentException under
# `powershell -File` here) and `Tee-Object` (writes UTF-16 in PS 5.1).

[CmdletBinding()]
param(
  [string]$Src      = "E:\a05c\s",
  [string]$Build    = "E:\a05c\b",
  [string]$LogDir   = "E:\claw4-a05-19fd979\logs",
  [string]$IdfPath  = "E:\workbuddy\esp-idf-5.5.4-ascii",
  [string]$IdfTools = "E:\workbuddy\claw4-idf-tools",
  [string]$PartitionCsv = "",
  [switch]$Fresh
)

# CP2-RUNNER-FIX-001: the approved partition CSV is taken from INSIDE the
# isolated source tree -- no default reference to E:\c any more.
if (-not $PartitionCsv) { $PartitionCsv = Join-Path $Src "partitions\v1\32m_dual.csv" }

# CP2-RUNNER-FIX-001: the five artifacts a CP2 run MUST produce. A CP2 build that
# exits 0 but leaves any of them missing is NOT a passing CP2 (exit 5).
$RequiredArtifacts = @("xiaozhi.bin", "xiaozhi.elf", "xiaozhi.map",
                       "bootloader\bootloader.bin", "partition_table\partition-table.bin")

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
Say "partition csv: $PartitionCsv"
$preHash = (Get-FileHash -LiteralPath $sdkconfig -Algorithm SHA256).Hash.ToLower()
Say ("sdkconfig sha256 (pre) : " + $preHash)
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

# ========= P0 guard: host PATH desynchronisation (fail closed) ==============
# Codex CP2 review 2026-09-15 §8 option 1 requires confirming, BEFORE building,
# that the Python that runs idf.py agrees with the real Win32 process PATH.
# If it does not, idf_py_actions/tools.py will hand cmake a PATH without the
# toolchain (env_copy = dict(os.environ)) and ninja will not be found.
Say "== P0 guard: os.environ['PATH'] vs real Win32 process PATH =="
$guardPy = Join-Path $LogDir "envdiag-pathguard.py"
@'
import ctypes, os, shutil, sys

need_dirs = [d for d in sys.argv[1:] if d]
buf = ctypes.create_unicode_buffer(65536)
ctypes.windll.kernel32.GetEnvironmentVariableW("PATH", buf, 65536)
real = buf.value
env = os.environ.get("PATH") or ""

problems = []
if env != real:
    problems.append("os.environ['PATH'] != Win32 GetEnvironmentVariableW('PATH')  (len %d vs %d)"
                    % (len(env), len(real)))
for d in need_dirs:
    if d.lower() not in env.lower():
        problems.append("tool dir absent from os.environ['PATH']: %s" % d)
for name in ("cmake", "ninja", "riscv32-esp-elf-g++"):
    if shutil.which(name) is None:
        problems.append("shutil.which(%r) is None" % name)

if problems:
    print("PATH_GUARD: DESYNC -- this process cannot see the toolchain; refusing to build")
    for p in problems:
        print("  - " + p)
    print("  fix: run this script from an ordinary host terminal")
    print("       (Codex CP2 review 2026-09-15 sec.8 option 1)")
    sys.exit(1)

print("PATH_GUARD: OK -- os.environ['PATH'] == Win32 PATH (len %d)" % len(env))
print("            all %d tool dirs present; cmake/ninja/riscv32-esp-elf-g++ all resolvable"
      % len(need_dirs))
sys.exit(0)
'@ | Set-Content -LiteralPath $guardPy -Encoding UTF8

& $idfPy $guardPy $prependAll *>&1 | ForEach-Object { Say ([string]$_) }
$guardRc = $LASTEXITCODE
Say ("path guard rc = " + $guardRc)
if ($guardRc -ne 0) {
  Say "HARD STOP: host PATH desynchronisation is STILL PRESENT -> no build attempted."
  Say "  Codex CP2 review 2026-09-15: RC = HOST ENVIRONMENT PATH DESYNCHRONIZATION"
  Say ("log: " + $log)
  exit 4
}
Say ""

# ===================== HARD precondition (fail closed) =====================
Say "== HARD precondition: verify-build-inputs.py --check =="
Say ("manifest : " + $Manifest)
Say ("verifier : " + $VerifyInputs)
if (-not (Test-Path -LiteralPath $Manifest)) { Say "FATAL manifest missing: $Manifest"; exit 3 }
if (-not (Test-Path -LiteralPath $VerifyInputs)) { Say "FATAL input verifier missing: $VerifyInputs"; exit 3 }

# --- source/tree binding (fail closed) -------------------------------------
# The 16:52 run proved the input check ALONE is not enough: it passed 5/5 while
# the build compiled a tree the manifest does not describe. Bind -Src to the
# manifest's isolated_src.origin here, before anything expensive happens.
function ConvertTo-NormPath([string]$p) {
  [System.IO.Path]::GetFullPath($p).TrimEnd([char]92).ToLowerInvariant()
}
Say "-- source/tree binding (-Src vs manifest isolated_src.origin) --"
try {
  $mj  = Get-Content -LiteralPath $Manifest -Raw -Encoding UTF8 | ConvertFrom-Json
  $iso = @($mj.entries | Where-Object { $_.id -eq "isolated_src" })
  if ($iso.Count -ne 1) { throw ("expected exactly 1 isolated_src entry, found " + $iso.Count) }
  $mOrigin = [string]$iso[0].origin
  Say ("  manifest isolated_src.origin : " + $mOrigin)
  Say ("  -Src (tree that would build)  : " + $Src)
  if ((ConvertTo-NormPath $mOrigin) -ne (ConvertTo-NormPath $Src)) {
    Say "HARD STOP: -Src is NOT the tree the frozen manifest describes."
    Say "  The input check cannot catch this (it validates the manifest's own paths)."
    Say ("  re-run with: -Src " + $mOrigin)
    Say ("log: " + $log)
    exit 3
  }
  Say "  OK -- -Src == manifest isolated_src.origin"
} catch {
  Say ("HARD STOP: source binding check failed: " + $_.Exception.Message)
  Say ("log: " + $log)
  exit 3
}
Say ""

# CP2-RUNNER-FIX-001 (fix 3, fail-closed): the approved partition CSV now lives
# inside the isolated source tree. Check it up front so a long build can never
# finish without the expected input for the section-9 item 14/15 comparison.
if (-not (Test-Path -LiteralPath $PartitionCsv)) {
  Say ("FATAL approved partition CSV missing: " + $PartitionCsv)
  Say ("log: " + $log)
  exit 3
}
$pcText = [System.IO.File]::ReadAllText($PartitionCsv)
$pcCrlf = [regex]::Matches($pcText, "\r\n").Count
$pcLfAll = [regex]::Matches($pcText, "\n").Count
$pcLfNorm = $pcText -replace "\r\n", ([string][char]10)
$pcShaRaw = (Get-FileHash -LiteralPath $PartitionCsv -Algorithm SHA256).Hash.ToLower()
$pcShaLf = [System.BitConverter]::ToString(
  ([System.Security.Cryptography.SHA256]::Create()).ComputeHash(
    [System.Text.Encoding]::UTF8.GetBytes($pcLfNorm))).Replace("-", "").ToLower()
Say ("partition csv raw sha256 : " + $pcShaRaw)
Say ("partition csv LF  sha256 : " + $pcShaLf + "   [CRLF=" + $pcCrlf + " bareLF=" + ($pcLfAll - $pcCrlf) + "]")

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

# --- post checks: artifacts (Codex CP2 review §9 items 7-13) ---------------
# CP2-RUNNER-FIX-001 (fix 1): the "post" hash is a RE-HASH of the SDKCONFIG that
# was actually passed in (-D SDKCONFIG=<abs>), taken after configure. It is no
# longer read from <build>\config\sdkconfig, which is a different file produced
# by the build; the point of item 13 is that configure must not rewrite the
# input we handed it.
$postHash = (Get-FileHash -LiteralPath $sdkconfig -Algorithm SHA256).Hash.ToLower()
Say ("sdkconfig sha256 (pre)  : " + $preHash)
Say ("sdkconfig sha256 (post) : " + $postHash)

# CP2-RUNNER-FIX-001 (fix 2): all five required artifacts are mandatory.
$missingArtifacts = @()
foreach ($a in $RequiredArtifacts) {
  $p = Join-Path $Build $a
  if (Test-Path -LiteralPath $p) {
    Say ("artifact {0,-38} {1,12} bytes  sha256={2}" -f $a, (Get-Item -LiteralPath $p).Length,
         (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLower())
  } else {
    Say ("artifact {0,-38} ABSENT" -f $a)
    $missingArtifacts += $a
  }
}
Say ("required artifacts present : " + ($RequiredArtifacts.Count - $missingArtifacts.Count) +
     " / " + $RequiredArtifacts.Count)
Say "NO flash / erase / monitor / flasher_args was executed."
Say "log: $log"
if ($rc -ne 0) { exit $rc }

# ===== post-build verification (Codex CP2 review §9 items 7-16) ============
Say ""
Say "== post-build verification (Codex CP2 review 2026-09-15 sec.9) =="
$postFail = @()

# items 7-11: every required artifact must exist
if ($missingArtifacts.Count -gt 0) {
  $postFail += ("[7-11] required artifact(s) missing: " + ($missingArtifacts -join ", "))
} else {
  Say "  [7-11] all 5 required artifacts present"
}

# item 13: the passed SDKCONFIG must not have been rewritten by configure
if ($postHash -ne $preHash) {
  $postFail += "[13] SDKCONFIG DRIFTED across configure ($preHash -> $postHash)"
} else {
  Say ("  [13] SDKCONFIG pre == post (" + $postHash + ") -- configure rewrote nothing")
}

Say "  [16] re-verify the external inputs AFTER the build"
& $idfPy $VerifyInputs --manifest $Manifest --check *>&1 | ForEach-Object { Say ("       " + [string]$_) }
if ($LASTEXITCODE -ne 0) {
  $postFail += "[16] external inputs drifted across the build"
} else {
  Say "       -> still 5/5 PASS"
}

$ptBin = Join-Path $Build "partition_table\partition-table.bin"
$appBin = Join-Path $Build "xiaozhi.bin"
$ptTool = Join-Path $RepoRoot "tools\dev\verify-partition-table.py"
if ((Test-Path -LiteralPath $ptBin) -and (Test-Path -LiteralPath $appBin) -and (Test-Path -LiteralPath $ptTool)) {
  Say "  [14/15] decode the GENERATED partition-table.bin and compare with the approved CSV"
  & $idfPy $ptTool --csv $PartitionCsv --bin $ptBin --app $appBin *>&1 | ForEach-Object { Say ("       " + [string]$_) }
  if ($LASTEXITCODE -ne 0) {
    $postFail += "[14/15] real partition / app verification FAILED"
  } else {
    Say "       -> real layout matches, no overlap, in range; headroom from THIS candidate"
  }
} else {
  $postFail += "[14/15] partition-table.bin, app bin or verifier missing -> real partition verification not possible"
}

Say ""
if ($postFail.Count -gt 0) {
  Say ("POST-BUILD VERIFICATION: FAIL (" + $postFail.Count + ")")
  foreach ($f in $postFail) { Say ("  - " + $f) }
  exit 5
}
Say "POST-BUILD VERIFICATION: PASS (items 7-16 satisfied; NO FLASH)"
exit 0
