[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$MetricsPath,
    [string]$ExpectedProcessName = 'FalloutNV',
    [ValidateRange(1, 86400)][int]$MinimumDurationSeconds = 7200,
    [ValidateRange(0, 7200)][int]$MinimumWarmupSeconds = 300,
    [ValidateRange(100, 60000)][int]$MaximumIntervalMilliseconds = 5000,
    [ValidateRange(1, 100)][int]$MinimumCoveragePercent = 80,
    [ValidateRange(1, [Int64]::MaxValue)][Int64]$MaximumPrivateGrowthBytes = 134217728,
    [ValidateRange(0, [Int32]::MaxValue)][int]$MaximumHandleGrowth = 32,
    [ValidateRange(0, [Int32]::MaxValue)][int]$MaximumThreadGrowth = 8
)

$ErrorActionPreference = 'Stop'

function Read-Int64 {
    param(
        [Parameter(Mandatory)]$Value,
        [Parameter(Mandatory)][string]$Name
    )
    $parsed = 0L
    if ($null -eq $Value -or
        -not [Int64]::TryParse(
            [string]$Value,
            [Globalization.NumberStyles]::Integer,
            [Globalization.CultureInfo]::InvariantCulture,
            [ref]$parsed) -or
        $parsed -lt 0) {
        throw "The process metrics contain an invalid $Name value."
    }
    return $parsed
}

function Get-Range {
    param(
        [Parameter(Mandatory)][object[]]$Samples,
        [Parameter(Mandatory)][string]$Property
    )
    $values = @($Samples | ForEach-Object {
        Read-Int64 -Value $_.$Property -Name $Property
    })
    $measurement = $values | Measure-Object -Minimum -Maximum
    return [ordered]@{
        initial = [Int64]$values[0]
        final = [Int64]$values[-1]
        minimum = [Int64]$measurement.Minimum
        maximum = [Int64]$measurement.Maximum
        delta = [Int64]$values[-1] - [Int64]$values[0]
    }
}

function Assert-Range {
    param(
        [Parameter(Mandatory)]$Actual,
        [Parameter(Mandatory)]$Expected,
        [Parameter(Mandatory)][string]$Property
    )
    foreach ($field in @('initial', 'final', 'minimum', 'maximum', 'delta')) {
        if ([Int64]$Actual.$field -ne [Int64]$Expected[$field]) {
            throw "The $Property summary does not match its raw samples."
        }
    }
}

$result = Get-Content -LiteralPath $MetricsPath -Raw | ConvertFrom-Json
if ($result.schema -ne 1) {
    throw 'The process metrics use an unsupported schema.'
}
if ([string]::IsNullOrWhiteSpace($result.process_name) -or
    -not [string]::Equals(
        $result.process_name,
        $ExpectedProcessName,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The process metrics identify the wrong process.'
}

$processId = Read-Int64 -Value $result.process_id -Name 'process_id'
$durationMs = Read-Int64 -Value $result.duration_ms -Name 'duration_ms'
$intervalMs = Read-Int64 -Value $result.interval_ms -Name 'interval_ms'
$warmupSeconds = Read-Int64 -Value $result.warmup_seconds -Name 'warmup_seconds'
if ($processId -eq 0) {
    throw 'The process metrics contain an invalid process ID.'
}
if ($durationMs -lt ([Int64]$MinimumDurationSeconds * 1000)) {
    throw 'The process metrics do not cover the required duration.'
}
if ($intervalMs -lt 100 -or $intervalMs -gt $MaximumIntervalMilliseconds) {
    throw 'The process metrics use an unacceptable sample interval.'
}
if ($warmupSeconds -lt $MinimumWarmupSeconds -or
    ([Int64]$warmupSeconds * 1000) -ge $durationMs) {
    throw 'The process metrics use an unacceptable warm-up period.'
}

$startedUtc = [DateTime]::MinValue
$endedUtc = [DateTime]::MinValue
if (-not [DateTime]::TryParse(
        [string]$result.started_utc,
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::RoundtripKind,
        [ref]$startedUtc) -or
    -not [DateTime]::TryParse(
        [string]$result.ended_utc,
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::RoundtripKind,
        [ref]$endedUtc) -or
    $endedUtc -le $startedUtc) {
    throw 'The process metrics contain invalid capture timestamps.'
}

$samples = @($result.samples)
$sampleCount = Read-Int64 -Value $result.sample_count -Name 'sample_count'
if ($sampleCount -ne $samples.Count -or $samples.Count -lt 2) {
    throw 'The process metrics contain an invalid sample count.'
}
$expectedSamples = [Math]::Max(1L, [Int64][Math]::Floor($durationMs / $intervalMs))
$minimumSamples = [Int64][Math]::Floor(
    $expectedSamples * ([double]$MinimumCoveragePercent / 100.0))
if ($sampleCount -lt $minimumSamples) {
    throw 'The process metrics do not meet the required sampling coverage.'
}

$previousElapsed = -1L
$previousCpu = -1L
foreach ($sample in $samples) {
    $elapsed = Read-Int64 -Value $sample.elapsed_ms -Name 'elapsed_ms'
    $cpu = Read-Int64 -Value $sample.cpu_ms -Name 'cpu_ms'
    foreach ($property in @('private_bytes', 'working_set_bytes', 'handles', 'threads')) {
        [void](Read-Int64 -Value $sample.$property -Name $property)
    }
    if ($elapsed -le $previousElapsed -or $elapsed -gt $durationMs) {
        throw 'The process metrics contain nonmonotonic or out-of-range elapsed times.'
    }
    if ($cpu -lt $previousCpu) {
        throw 'The process metrics contain nonmonotonic CPU time.'
    }
    $previousElapsed = $elapsed
    $previousCpu = $cpu
}

$warmupMilliseconds = [Int64]$warmupSeconds * 1000
$analysisSamples = @($samples | Where-Object {
    [Int64]$_.elapsed_ms -ge $warmupMilliseconds
})
$analyzedCount = Read-Int64 `
    -Value $result.analyzed_sample_count `
    -Name 'analyzed_sample_count'
if ($analysisSamples.Count -lt 2 -or $analyzedCount -ne $analysisSamples.Count) {
    throw 'The process metrics contain an invalid analyzed sample count.'
}

$ranges = [ordered]@{}
foreach ($property in @(
        'private_bytes', 'working_set_bytes', 'handles', 'threads', 'cpu_ms')) {
    $ranges[$property] = Get-Range -Samples $analysisSamples -Property $property
    Assert-Range `
        -Actual $result.$property `
        -Expected $ranges[$property] `
        -Property $property
}

$privateGrowth = $ranges.private_bytes.maximum - $ranges.private_bytes.initial
$handleGrowth = $ranges.handles.maximum - $ranges.handles.initial
$threadGrowth = $ranges.threads.maximum - $ranges.threads.initial
if ($privateGrowth -ge $MaximumPrivateGrowthBytes) {
    throw 'The process private-memory growth reached the acceptance limit.'
}
if ($handleGrowth -gt $MaximumHandleGrowth) {
    throw 'The process handle growth exceeded the acceptance limit.'
}
if ($threadGrowth -gt $MaximumThreadGrowth) {
    throw 'The process thread growth exceeded the acceptance limit.'
}

Write-Host (
    ('Phase 6 process metrics passed: duration_ms={0} samples={1} ' +
    'private_growth={2} handle_growth={3} thread_growth={4}') -f
    $durationMs,
    $sampleCount,
    $privateGrowth,
    $handleGrowth,
    $threadGrowth)
