[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SamplerPath
)

$ErrorActionPreference = 'Stop'
$sampler = (Resolve-Path -LiteralPath $SamplerPath).Path
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "pbvp-process-metrics-test-$([Guid]::NewGuid().ToString('N'))")
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
$output = Join-Path $temporaryRoot 'metrics.json'
$child = $null

try {
    $powershell = (Get-Process -Id $PID).Path
    $child = Start-Process `
        -FilePath $powershell `
        -ArgumentList @(
            '-NoProfile',
            '-Command',
            'Start-Sleep -Milliseconds 1500') `
        -PassThru `
        -WindowStyle Hidden
    & $sampler `
        -TargetProcessId $child.Id `
        -OutputPath $output `
        -IntervalMilliseconds 100 `
        -WaitTimeoutSeconds 5 `
        -WarmupSeconds 0 | Out-Null
    $result = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
    if ($result.schema -ne 1 -or $result.process_id -ne $child.Id) {
        throw 'The sampler wrote the wrong schema or process ID.'
    }
    if ($result.sample_count -lt 5 -or
        $result.analyzed_sample_count -ne $result.sample_count -or
        $result.duration_ms -lt 500) {
        throw 'The sampler did not capture the expected process lifetime.'
    }
    foreach ($metric in @(
            'private_bytes', 'working_set_bytes', 'handles', 'threads', 'cpu_ms')) {
        $range = $result.$metric
        if ($range.minimum -lt 0 -or $range.maximum -lt $range.minimum -or
            $range.initial -lt $range.minimum -or $range.initial -gt $range.maximum -or
            $range.final -lt $range.minimum -or $range.final -gt $range.maximum) {
            throw "The sampler wrote an invalid $metric range."
        }
    }
    Write-Host 'Phase 6 process metrics tests passed.'
} finally {
    if ($null -ne $child -and -not $child.HasExited) {
        Stop-Process -Id $child.Id -Force
        $child.WaitForExit()
    }
    if ((Test-Path -LiteralPath $temporaryRoot) -and
        [IO.Path]::GetFileName($temporaryRoot) -like 'pbvp-process-metrics-test-*' -and
        (Split-Path -Parent $temporaryRoot) -eq [IO.Path]::GetTempPath().TrimEnd('\')) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
