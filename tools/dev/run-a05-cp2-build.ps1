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

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$log = Join-Path $LogDir ("cp2-build-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".log")
$script:fail = $null

function Say($m) {
  $line = [string]$m
  Write-Host $line
  Add-Content -Path $log -Value $line -Encoding UTF8
}

Say "== A05 CP2 build =="
Say ("started      : " + (Get-Date -Format s))

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

$prepend = @(
  (Join-Path $IdfTools "python_env\idf5.5_py3.12_env\Scripts"),
  (Join-Path $IdfTools "tools\cmake\3.30.2\bin"),
  (Join-Path $IdfTools "tools\ninja\1.12.1"),
  (Join-Path $IdfTools "tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin")
) | Where-Object { Test-Path -LiteralPath $_ }
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
