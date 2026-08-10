[CmdletBinding()]
param([Parameter(Mandatory)][string]$CheckerPath)

$ErrorActionPreference = 'Stop'
$checker = (Resolve-Path -LiteralPath $CheckerPath).Path
$shell = (Get-Process -Id $PID).Path
$tempRoot = [IO.Path]::GetFullPath(
    [IO.Path]::Combine([IO.Path]::GetTempPath(), "pbvp-phase1-log-$([Guid]::NewGuid().ToString('N'))"))
[IO.Directory]::CreateDirectory($tempRoot) | Out-Null

function Write-Fixture {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Text)
    $path = Join-Path $tempRoot $Name
    $normalized = $Text.Replace("`r`n", "`n").Replace("`r", "`n").Replace("`n", "`r`n")
    [IO.File]::WriteAllText($path, $normalized)
    return $path
}

function Invoke-Checker {
    param(
        [Parameter(Mandatory)][string]$Fixture,
        [string[]]$Arguments = @(),
        [switch]$ReportFailure
    )
    $output = @(& $shell -NoProfile -ExecutionPolicy Bypass -File $checker -LogPath $Fixture @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($ReportFailure -and $exitCode -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
    }
    return $exitCode
}

try {
    $base = @'
12:00:00.000 [INFO] Pip-Boy Video Player 0.1.0 loading; runtime=0x040020D0 xNVSE=0x06040050
12:00:00.010 [INFO] xNVSE frame-present presentation path enabled without executable hooks
12:00:01.000 [INFO] UIO video rectangle resolved: left=42.00 top=375.00 right=426.00 bottom=591.00 canvas=1706.67x960.00
12:00:01.010 [INFO] D3D device validated: adapter=Fixture driver=fixture.dll mode=fullscreen backbuffer=1920x1080 format=22 interval=0x00000001
12:00:01.020 [INFO] Engine texture checkerboard upload took 24.50 microseconds
12:00:01.021 [INFO] Generated checkerboard uploaded to PBVP_VideoSurface
12:00:04.021 [INFO] Visible frame cadence: frames=181 elapsed-ms=3000.00 fps=60.00
12:00:07.021 [INFO] Visible frame cadence: frames=181 elapsed-ms=3000.00 fps=60.00
12:00:07.900 [INFO] Phase 1 renderer summary: callbacks=400 visible=362 devices=1 upload-successes=1 upload-attempts=1 upload-failures=0 upload-us=24.50/24.50/24.50
12:00:07.901 [INFO] Phase 1 cadence summary: samples=2 fps=60.00/60.00/60.00
12:00:08.000 [INFO] Process shutdown requested
'@
    $normal = Write-Fixture -Name 'normal.txt' -Text $base
    if ((Invoke-Checker -Fixture $normal -Arguments @(
            '-ExpectedWidth', '1920', '-ExpectedHeight', '1080',
            '-ExpectedFps', '60', '-RequireCleanExit') -ReportFailure) -ne 0) {
        throw 'The valid normal fixture failed.'
    }

    $errorFixture = Write-Fixture -Name 'error.txt' -Text (
        $base + "12:00:01.500 [ERROR] Fixture error`n")
    if ((Invoke-Checker -Fixture $errorFixture) -eq 0) {
        throw 'The error fixture was accepted.'
    }
    if ((Invoke-Checker -Fixture $normal -Arguments @(
            '-ExpectedWidth', '1280', '-ExpectedHeight', '720')) -eq 0) {
        throw 'The wrong backbuffer fixture was accepted.'
    }
    if ((Invoke-Checker -Fixture $normal -Arguments @('-ExpectedFps', '30')) -eq 0) {
        throw 'The wrong visible cadence was accepted.'
    }

    $missingSummary = Write-Fixture -Name 'missing-summary.txt' -Text (
        ($base -split "`n" | Where-Object { $_ -notmatch 'Phase 1 renderer summary:' }) -join "`n")
    if ((Invoke-Checker -Fixture $missingSummary -Arguments @('-RequireCleanExit')) -eq 0) {
        throw 'The clean-exit fixture without a renderer summary was accepted.'
    }

    $inconsistentSummary = Write-Fixture -Name 'inconsistent-summary.txt' -Text (
        $base.Replace('devices=1 upload-successes=1', 'devices=2 upload-successes=1'))
    if ((Invoke-Checker -Fixture $inconsistentSummary -Arguments @('-RequireCleanExit')) -eq 0) {
        throw 'The inconsistent renderer summary was accepted.'
    }

    $inconsistentCadence = Write-Fixture -Name 'inconsistent-cadence.txt' -Text (
        $base.Replace(
            'frames=181 elapsed-ms=3000.00 fps=60.00',
            'frames=181 elapsed-ms=3000.00 fps=120.00'))
    if ((Invoke-Checker -Fixture $inconsistentCadence) -eq 0) {
        throw 'The inconsistent cadence record was accepted.'
    }

    Write-Host 'Phase 1 log checker tests passed.'
} finally {
    $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
    $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedTemp)) {
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    }
}
