param([Parameter(Mandatory=$true)][string]$CompilerPath,
      [string]$SourceRoot = (Join-Path $PSScriptRoot '../../..'),
      [string]$OutputDir = (Join-Path $PSScriptRoot '../../../out/nvs-read-contracts'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../invoke-host-test.ps1')
$env:PATH = (Split-Path -Parent $CompilerPath) + ';' + $env:PATH
New-Item -ItemType Directory -Force $OutputDir | Out-Null
$fixture = Join-Path $PSScriptRoot 'fixtures/nvs'
$exe = Join-Path (Resolve-Path $OutputDir).Path 'nvs_read_contract_tests.exe'
$sources = @('integration/metalio_claw4/device/ports/nvs_outbox_storage.cpp',
             'integration/metalio_claw4/device/ports/nvs_backend_provisioning.cpp',
             'integration/metalio_claw4/device/core/outbox_codec.cpp') |
    ForEach-Object { Join-Path $SourceRoot $_ }
$global:LASTEXITCODE = -1
& $CompilerPath -std=c++17 -Wall -Wextra -Werror -I $fixture `
    -I (Join-Path $SourceRoot 'firmware/main') -I (Join-Path $SourceRoot 'integration') `
    @sources (Join-Path $fixture 'read_contract_tests.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw "NVS shim compile failed: $LASTEXITCODE" }
$result = Invoke-HostTest $exe
$result.Output
if ($result.ExitCode -ne 0) { throw "NVS shim run failed: $($result.ExitCode)" }
