# A05 CP2: full ESP-IDF build in the isolated root.
#
# Constraints honoured (WB-A05-BUILD-001 §3 + reviewer rulings):
#   * a NEW isolated build root; no reuse of any old CMakeCache/object/ELF
#   * `idf.py build` only -- no copied ninja invocation, no set-target/menuconfig
#   * SDKCONFIG passed EXPLICITLY; its hash is recorded before and after configure
#   * ESP_IDF_VERSION=5.5 (NOT 5.5.4 -- 5.5.4 silently drops the esp_wifi_remote
#     Kconfig variant and loses the Wi-Fi symbols; documented in
#     WB-APP-FIRST-L3_BUILD_STALE_FIX_2026-09-12 and CODEX_APP_FIRST_CONTINUATION)
#   * PYTHONPATH and CODEBUDDY_SAFE_DELETE_* stripped (they break asset generation)
#   * NO flash / app-flash / erase / monitor; no flasher_args execution

[CmdletBinding()]
param(
  [string]$Src      = "E:\claw4-a05-19fd979\src",
  [string]$Build    = "E:\claw4-a05-19fd979\build",
  [string]$LogDir   = "E:\claw4-a05-19fd979\logs",
  [string]$IdfPath  = "E:\workbuddy\esp-idf-5.5.4-ascii",
  [string]$IdfTools = "E:\workbuddy\claw4-idf-tools",
  [switch]$Fresh
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$log = Join-Path $LogDir "cp2-build-$(Get-Date -Format yyyyMMdd-HHmmss).log"
function Say($m) { $m | Tee-Object -FilePath $log -Append }

Say "== A05 CP2 build =="
Say ("started      : " + (Get-Date -Format s))

# --- preconditions ---------------------------------------------------------
foreach ($p in @($Src, $IdfPath, $IdfTools)) {
  if (-not (Test-Path $p)) { throw "missing required path: $p" }
}
if ($Fresh -and (Test-Path $Build)) {
  Say "removing existing build root (explicit -Fresh): $Build"
  Remove-Item -Recurse -Force $Build
  if (Test-Path $Build) { throw "build root still present after removal: $Build" }
}
New-Item -ItemType Directory -Force -Path $Build | Out-Null

$sdkconfig = Join-Path $Src "sdkconfig"
if (-not (Test-Path $sdkconfig)) { throw "SDKCONFIG not found: $sdkconfig" }

# --- environment ----------------------------------------------------------
$env:IDF_PATH                = $IdfPath
$env:IDF_TOOLS_PATH          = $IdfTools
$env:IDF_PYTHON_ENV_PATH     = Join-Path $IdfTools "python_env\idf5.5_py3.12_env"
$env:ESP_IDF_VERSION         = "5.5"       # REQUIRED value; see header note
$env:IDF_VERSION             = "5.5.4"
Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
Get-ChildItem Env: | Where-Object { $_.Name -like "CODEBUDDY_SAFE_DELETE*" } | ForEach-Object {
  Remove-Item ("Env:" + $_.Name) -ErrorAction SilentlyContinue
}
$prepend = @(
  (Join-Path $IdfTools "python_env\idf5.5_py3.12_env\Scripts"),
  (Join-Path $IdfTools "tools\cmake\3.30.2\bin"),
  (Join-Path $IdfTools "tools\ninja\1.12.1"),
  (Join-Path $IdfTools "tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin")
) | Where-Object { Test-Path $_ }
$env:PATH = ($prepend -join ";") + ";" + $env:PATH

$idfPy = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
Say "idf python   : $idfPy"
Say "IDF_PATH     : $env:IDF_PATH"
Say "ESP_IDF_VERSION=$($env:ESP_IDF_VERSION)  IDF_VERSION=$($env:IDF_VERSION)"
Say "source       : $Src"
Say "build        : $Build"
Say "sdkconfig    : $sdkconfig"
Say ("sdkconfig sha256 (pre) : " + (Get-FileHash $sdkconfig -Algorithm SHA256).Hash.ToLower())

# --- configure + build ----------------------------------------------------
$cmd = @(
  $idfPy, (Join-Path $IdfPath "tools\idf.py"),
  "-C", $Src,
  "-B", $Build,
  "-D", ("SDKCONFIG=" + $sdkconfig),
  "build"
)
Say "command      : idf.py -C <src> -B <build> -D SDKCONFIG=<abs> build"
Say "start=" + (Get-Date -Format s)
Say ""

& $cmd[0] $cmd[1..($cmd.Count-1)] 2>&1 | Tee-Object -FilePath $log -Append
$rc = $LASTEXITCODE
Say ""
Say "exit=$rc  end=" + (Get-Date -Format s)

# --- post checks (report only; NO flash) ---------------------------------
$sdkAfter = Join-Path $Build "config\sdkconfig"
Say ("sdkconfig sha256 (post-configure, build/config/sdkconfig) : " + `
     $(if (Test-Path $sdkAfter) { (Get-FileHash $sdkAfter -Algorithm SHA256).Hash.ToLower() } else { "ABSENT" }))

foreach ($a in @("xiaozhi.bin","xiaozhi.elf","xiaozhi.map","bootloader\bootloader.bin","partition_table\partition-table.bin")) {
  $p = Join-Path $Build $a
  if (Test-Path $p) {
    Say ("artifact {0,-40} {1,12} bytes  sha256={2}" -f $a, (Get-Item $p).Length, (Get-FileHash $p -Algorithm SHA256).Hash.ToLower())
  } else {
    Say ("artifact {0,-40} ABSENT" -f $a)
  }
}
Say "NO flash / erase / monitor / flasher_args was executed."
Say "log: $log"
exit $rc
