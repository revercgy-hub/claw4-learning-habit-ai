# Launch failures must never inherit a previous compiler's successful exit code.
function Invoke-HostTest([string]$Executable) {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $Executable
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
        if (-not $process.Start()) { throw 'Process did not start' }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        return @{ ExitCode = $process.ExitCode; Output = ($stdout.Result + $stderr.Result) }
    } catch {
        return @{ ExitCode = -1; Output = "LAUNCH FAIL: $($_.Exception.Message)" }
    } finally {
        $process.Dispose()
    }
}
