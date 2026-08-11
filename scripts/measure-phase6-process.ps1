[CmdletBinding()]
param(
    [string]$ProcessName = 'FalloutNV',
    [int]$TargetProcessId = 0,
    [Parameter(Mandatory)][string]$OutputPath,
    [ValidateRange(100, 60000)][int]$IntervalMilliseconds = 5000,
    [ValidateRange(1, 3600)][int]$WaitTimeoutSeconds = 300,
    [ValidateRange(0, 7200)][int]$WarmupSeconds = 300
)

$ErrorActionPreference = 'Stop'

function Find-TargetProcess {
    if ($TargetProcessId -gt 0) {
        return Get-Process -Id $TargetProcessId -ErrorAction SilentlyContinue
    }
    return Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
        Sort-Object StartTime |
        Select-Object -Last 1
}

function Get-RangeSummary {
    param(
        [Parameter(Mandatory)][object[]]$Samples,
        [Parameter(Mandatory)][string]$Property
    )
    $measurement = $Samples | Measure-Object -Property $Property -Minimum -Maximum
    return [ordered]@{
        initial = [Int64]$Samples[0].$Property
        final = [Int64]$Samples[-1].$Property
        minimum = [Int64]$measurement.Minimum
        maximum = [Int64]$measurement.Maximum
        delta = [Int64]$Samples[-1].$Property - [Int64]$Samples[0].$Property
    }
}

$wait = [Diagnostics.Stopwatch]::StartNew()
$process = $null
while ($null -eq $process -and $wait.Elapsed.TotalSeconds -lt $WaitTimeoutSeconds) {
    $process = Find-TargetProcess
    if ($null -eq $process) {
        Start-Sleep -Milliseconds ([Math]::Min($IntervalMilliseconds, 1000))
    }
}
if ($null -eq $process) {
    throw 'The target process did not start before the wait timeout.'
}

$samples = [Collections.Generic.List[object]]::new()
$capture = [Diagnostics.Stopwatch]::StartNew()
$startedUtc = [DateTime]::UtcNow
while ($true) {
    try {
        $process.Refresh()
        if ($process.HasExited) { break }
        $samples.Add([pscustomobject][ordered]@{
            elapsed_ms = [Int64]$capture.ElapsedMilliseconds
            private_bytes = [Int64]$process.PrivateMemorySize64
            working_set_bytes = [Int64]$process.WorkingSet64
            handles = [Int64]$process.HandleCount
            threads = [Int64]$process.Threads.Count
            cpu_ms = [Int64]$process.TotalProcessorTime.TotalMilliseconds
        })
    } catch [InvalidOperationException] {
        break
    } catch [System.ComponentModel.Win32Exception] {
        break
    }
    Start-Sleep -Milliseconds $IntervalMilliseconds
}

if ($samples.Count -eq 0) {
    throw 'The target process exited before a sample could be recorded.'
}
$endedUtc = [DateTime]::UtcNow
$warmSamples = @($samples | Where-Object {
    $_.elapsed_ms -ge ([Int64]$WarmupSeconds * 1000)
})
$analysisSamples = if ($warmSamples.Count -gt 0) { $warmSamples } else { @($samples) }
$summary = [ordered]@{
    schema = 1
    process_name = $process.ProcessName
    process_id = [Int64]$process.Id
    started_utc = $startedUtc.ToString('o')
    ended_utc = $endedUtc.ToString('o')
    duration_ms = [Int64]$capture.ElapsedMilliseconds
    interval_ms = $IntervalMilliseconds
    warmup_seconds = $WarmupSeconds
    sample_count = $samples.Count
    analyzed_sample_count = $analysisSamples.Count
    private_bytes = Get-RangeSummary -Samples $analysisSamples -Property 'private_bytes'
    working_set_bytes = Get-RangeSummary -Samples $analysisSamples -Property 'working_set_bytes'
    handles = Get-RangeSummary -Samples $analysisSamples -Property 'handles'
    threads = Get-RangeSummary -Samples $analysisSamples -Property 'threads'
    cpu_ms = Get-RangeSummary -Samples $analysisSamples -Property 'cpu_ms'
    samples = @($samples)
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $resolvedOutput
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
[IO.File]::WriteAllText(
    $resolvedOutput,
    ($summary | ConvertTo-Json -Depth 6),
    [Text.UTF8Encoding]::new($false))
Write-Host "Phase 6 process metrics written: samples=$($samples.Count) duration_ms=$($summary.duration_ms)"
