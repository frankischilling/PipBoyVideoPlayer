[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$checker = (Resolve-Path -LiteralPath $CheckerPath).Path
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "pbvp-process-metrics-checker-$([Guid]::NewGuid().ToString('N'))")
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null

function Update-Summary {
    param([Parameter(Mandatory)]$Fixture)
    $analysis = @($Fixture.samples | Where-Object {
        [Int64]$_.elapsed_ms -ge ([Int64]$Fixture.warmup_seconds * 1000)
    })
    $Fixture.sample_count = @($Fixture.samples).Count
    $Fixture.analyzed_sample_count = $analysis.Count
    foreach ($property in @(
            'private_bytes', 'working_set_bytes', 'handles', 'threads', 'cpu_ms')) {
        $values = @($analysis | ForEach-Object { [Int64]$_.$property })
        $measurement = $values | Measure-Object -Minimum -Maximum
        $Fixture.$property = [pscustomobject][ordered]@{
            initial = $values[0]
            final = $values[-1]
            minimum = [Int64]$measurement.Minimum
            maximum = [Int64]$measurement.Maximum
            delta = [Int64]$values[-1] - [Int64]$values[0]
        }
    }
}

function Copy-Fixture {
    param([Parameter(Mandatory)]$Fixture)
    return $Fixture | ConvertTo-Json -Depth 6 | ConvertFrom-Json
}

function Write-Fixture {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)]$Fixture
    )
    $path = Join-Path $temporaryRoot "$Name.json"
    [IO.File]::WriteAllText(
        $path,
        ($Fixture | ConvertTo-Json -Depth 6),
        [Text.UTF8Encoding]::new($false))
    return $path
}

try {
    $samples = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt 20; $index++) {
        $samples.Add([pscustomobject][ordered]@{
            elapsed_ms = $index * 100
            private_bytes = 1000000 + ($index * 1000)
            working_set_bytes = 2000000 + ($index * 2000)
            handles = 100 + [Math]::Min($index, 2)
            threads = 8 + [Math]::Min($index, 1)
            cpu_ms = $index * 5
        })
    }
    $good = [pscustomobject][ordered]@{
        schema = 1
        process_name = 'FalloutNV'
        process_id = 1234
        started_utc = '2026-08-11T12:00:00.0000000Z'
        ended_utc = '2026-08-11T12:00:02.0000000Z'
        duration_ms = 2000
        interval_ms = 100
        warmup_seconds = 0
        sample_count = 0
        analyzed_sample_count = 0
        private_bytes = $null
        working_set_bytes = $null
        handles = $null
        threads = $null
        cpu_ms = $null
        samples = @($samples)
    }
    Update-Summary -Fixture $good
    $goodPath = Write-Fixture -Name 'good' -Fixture $good
    & $checker `
        -MetricsPath $goodPath `
        -MinimumDurationSeconds 1 `
        -MinimumWarmupSeconds 0 `
        -MaximumIntervalMilliseconds 100 `
        -MinimumCoveragePercent 80 `
        -MaximumPrivateGrowthBytes 100000 `
        -MaximumHandleGrowth 4 `
        -MaximumThreadGrowth 2

    $badCases = [ordered]@{}

    $badCases.Schema = Copy-Fixture $good
    $badCases.Schema.schema = 2

    $badCases.Process = Copy-Fixture $good
    $badCases.Process.process_name = 'notepad'

    $badCases.Duration = Copy-Fixture $good
    $badCases.Duration.duration_ms = 999

    $badCases.Interval = Copy-Fixture $good
    $badCases.Interval.interval_ms = 101

    $badCases.Coverage = Copy-Fixture $good
    $badCases.Coverage.duration_ms = 3000

    $badCases.Elapsed = Copy-Fixture $good
    $badCases.Elapsed.samples[10].elapsed_ms = $badCases.Elapsed.samples[9].elapsed_ms
    Update-Summary -Fixture $badCases.Elapsed

    $badCases.Cpu = Copy-Fixture $good
    $badCases.Cpu.samples[10].cpu_ms = 0
    Update-Summary -Fixture $badCases.Cpu

    $badCases.Memory = Copy-Fixture $good
    $badCases.Memory.samples[19].private_bytes = 1100000
    Update-Summary -Fixture $badCases.Memory

    $badCases.Handles = Copy-Fixture $good
    $badCases.Handles.samples[19].handles = 105
    Update-Summary -Fixture $badCases.Handles

    $badCases.Threads = Copy-Fixture $good
    $badCases.Threads.samples[19].threads = 11
    Update-Summary -Fixture $badCases.Threads

    $badCases.Summary = Copy-Fixture $good
    $badCases.Summary.private_bytes.maximum++

    $badCases.Count = Copy-Fixture $good
    $badCases.Count.sample_count++

    $badCases.Timestamps = Copy-Fixture $good
    $badCases.Timestamps.ended_utc = $badCases.Timestamps.started_utc

    foreach ($case in $badCases.GetEnumerator()) {
        $path = Write-Fixture -Name $case.Key -Fixture $case.Value
        $rejected = $false
        try {
            & $checker `
                -MetricsPath $path `
                -MinimumDurationSeconds 1 `
                -MinimumWarmupSeconds 0 `
                -MaximumIntervalMilliseconds 100 `
                -MinimumCoveragePercent 80 `
                -MaximumPrivateGrowthBytes 100000 `
                -MaximumHandleGrowth 4 `
                -MaximumThreadGrowth 2
        } catch {
            $rejected = $true
        }
        if (-not $rejected) {
            throw "The process metrics checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 6 process metrics checker tests passed.'
} finally {
    if ((Test-Path -LiteralPath $temporaryRoot) -and
        [IO.Path]::GetFileName($temporaryRoot) -like 'pbvp-process-metrics-checker-*' -and
        (Split-Path -Parent $temporaryRoot) -eq [IO.Path]::GetTempPath().TrimEnd('\')) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
