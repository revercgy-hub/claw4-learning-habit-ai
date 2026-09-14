$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../invoke-host-test.ps1')
$global:LASTEXITCODE = 0
$missing = Join-Path $env:TEMP ([guid]::NewGuid().ToString() + '.exe')
$result = Invoke-HostTest $missing
if ($result.ExitCode -eq 0 -or $result.Output -notlike 'LAUNCH FAIL:*') {
    throw 'Missing executable incorrectly passed'
}
$result = Invoke-HostTest (Join-Path $env:SystemRoot 'System32/hostname.exe')
if ($result.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($result.Output)) {
    throw 'Valid executable did not run'
}
Write-Output 'Host launch failure and successful execution: PASS'
