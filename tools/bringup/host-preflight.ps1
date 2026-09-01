[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'

function Get-CommandSummary {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        return [pscustomobject]@{ Item = $Name; Status = 'MISSING'; Detail = 'Not found on PATH' }
    }

    return [pscustomobject]@{ Item = $Name; Status = 'FOUND'; Detail = $command.Source }
}

function Get-FirstOutputLine {
    param([Parameter(Mandatory = $true)][string]$Command)

    try {
        return (& $Command --version 2>&1 | Select-Object -First 1 | Out-String).Trim()
    }
    catch {
        return "Unable to run: $($_.Exception.Message)"
    }
}

$officialRepo = Join-Path $ProjectRoot 'vendor\MetalioClaw4'

Write-Output '=== CLAW4 HOST PREFLIGHT ==='
Write-Output "Project root: $ProjectRoot"
Write-Output "Official repo: $officialRepo"
Write-Output ''

$checks = @(
    Get-CommandSummary -Name 'git'
    Get-CommandSummary -Name 'python'
    Get-CommandSummary -Name 'idf.py'
)
$checks | Format-Table -AutoSize

Write-Output "IDF_PATH: $($env:IDF_PATH)"
Write-Output "Git version: $(Get-FirstOutputLine -Command 'git')"
if (Get-Command python -ErrorAction SilentlyContinue) {
    Write-Output "Python version: $(Get-FirstOutputLine -Command 'python')"
}
if (Get-Command idf.py -ErrorAction SilentlyContinue) {
    Write-Output "ESP-IDF version: $(Get-FirstOutputLine -Command 'idf.py')"
}

if (Test-Path -LiteralPath $officialRepo) {
    Write-Output 'Official repository: FOUND'
    $sdkconfig = Join-Path $officialRepo 'sdkconfig'
    if (Test-Path -LiteralPath $sdkconfig) {
        $target = Select-String -LiteralPath $sdkconfig -Pattern '^CONFIG_IDF_TARGET=' | Select-Object -First 1
        $partition = Select-String -LiteralPath $sdkconfig -Pattern '^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=' | Select-Object -First 1
        Write-Output "Configured target: $($target.Line)"
        Write-Output "Configured partition: $($partition.Line)"
    }
}
else {
    Write-Output 'Official repository: MISSING'
}

$serialPorts = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
if ($serialPorts.Count -eq 0) {
    Write-Output 'Serial ports: none currently detected (expected before the device is connected).'
}
else {
    Write-Output "Serial ports: $($serialPorts -join ', ')"
    Write-Output 'After Claw4 is connected, identify the P4 port by the Windows descriptor: USB JTAG/serial debug unit.'
}

Write-Output ''
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    Write-Output 'RESULT: NOT READY — install and export ESP-IDF v5.5.4, then rerun this script.'
}
elseif (-not (Test-Path -LiteralPath $officialRepo)) {
    Write-Output 'RESULT: NOT READY — official repository is unavailable.'
}
else {
    Write-Output 'RESULT: HOST READY FOR OFFICIAL BASELINE BUILD — device verification still pending.'
}
