[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath,
    [ValidateRange(0, 16384)][int]$ExpectedWidth = 0,
    [ValidateRange(0, 16384)][int]$ExpectedHeight = 0,
    [ValidateRange(0, 100000)][int]$MinimumUploads = 1,
    [ValidateRange(0, 100000)][int]$MinimumRecreates = 0,
    [switch]$RequireCleanExit
)

$ErrorActionPreference = 'Stop'

function Get-Matches {
    param(
        [Parameter(Mandatory)][string[]]$Lines,
        [Parameter(Mandatory)][string]$Pattern
    )
    return @($Lines | Select-String -Pattern $Pattern -CaseSensitive)
}

try {
    $resolved = (Resolve-Path -LiteralPath $LogPath).Path
    $file = Get-Item -LiteralPath $resolved
    if ($file.Length -gt 4MB) {
        throw 'The log is larger than the 4 MB diagnostic limit.'
    }
    $lines = @(Get-Content -LiteralPath $resolved)
    $failures = [System.Collections.Generic.List[string]]::new()

    $loads = Get-Matches -Lines $lines -Pattern '\[INFO\] Pip-Boy Video Player .+ loading; runtime='
    $hooks = Get-Matches -Lines $lines -Pattern '\[INFO\] Verified NiDX9Renderer::Recreate hook installed$'
    $rectangles = Get-Matches -Lines $lines -Pattern '\[INFO\] UIO video rectangle resolved:'
    $devices = Get-Matches -Lines $lines -Pattern '\[INFO\] D3D device validated:'
    $uploads = Get-Matches -Lines $lines -Pattern '\[INFO\] Generated checkerboard uploaded to PBVP_VideoSurface$'
    $uploadTimes = Get-Matches -Lines $lines -Pattern '\[INFO\] Engine texture checkerboard upload took ([0-9]+(?:\.[0-9]+)?) microseconds$'
    $recreateStarts = Get-Matches -Lines $lines -Pattern '\[INFO\] Transient engine-surface state cleared before engine recreation '
    $recreateSuccesses = @(
        Get-Matches -Lines $lines -Pattern '\[INFO\] D3D engine recreation (?:recovered the original|applied the requested) presentation parameters; resources will be reacquired$'
    )
    $errors = Get-Matches -Lines $lines -Pattern '\[ERROR\]'
    $resetFailures = Get-Matches -Lines $lines -Pattern '\[(?:WARN|ERROR)\] D3D engine recreation (?:failed|returned)'
    $summaryPattern = '\[INFO\] Phase 1 renderer summary: callbacks=(?<callbacks>[0-9]+) visible=(?<visible>[0-9]+) devices=(?<devices>[0-9]+) upload-successes=(?<uploadSuccesses>[0-9]+) upload-attempts=(?<uploadAttempts>[0-9]+) upload-failures=(?<uploadFailures>[0-9]+) upload-us=[0-9]+(?:\.[0-9]+)?/[0-9]+(?:\.[0-9]+)?/[0-9]+(?:\.[0-9]+)? recreation-successes=(?<recreateSuccesses>[0-9]+) recreation-starts=(?<recreateStarts>[0-9]+) recreation-failures=(?<recreateFailures>[0-9]+)$'
    $summaries = Get-Matches -Lines $lines -Pattern $summaryPattern
    $shutdowns = Get-Matches -Lines $lines -Pattern '\[INFO\] Process shutdown requested$'

    if ($loads.Count -lt 1) { $failures.Add('Plugin load record is missing.') }
    if ($hooks.Count -lt 1) { $failures.Add('Verified reset hook record is missing.') }
    if ($rectangles.Count -lt 1) { $failures.Add('Resolved UIO rectangle record is missing.') }
    if ($devices.Count -lt 1) { $failures.Add('Validated Direct3D device record is missing.') }
    if ($uploads.Count -lt $MinimumUploads) {
        $failures.Add("Expected at least $MinimumUploads successful uploads, found $($uploads.Count).")
    }
    if ($errors.Count -gt 0) { $failures.Add("The log contains $($errors.Count) error records.") }
    if ($resetFailures.Count -gt 0) {
        $failures.Add("The log contains $($resetFailures.Count) failed or rejected recreation records.")
    }
    if ($recreateStarts.Count -lt $MinimumRecreates) {
        $failures.Add("Expected at least $MinimumRecreates recreation starts, found $($recreateStarts.Count).")
    }
    if ($recreateSuccesses.Count -lt $MinimumRecreates) {
        $failures.Add("Expected at least $MinimumRecreates recreation successes, found $($recreateSuccesses.Count).")
    }
    if ($RequireCleanExit -and $shutdowns.Count -lt 1) {
        $failures.Add('Clean process shutdown record is missing.')
    }
    if ($RequireCleanExit -and $summaries.Count -ne 1) {
        $failures.Add("Expected one renderer session summary, found $($summaries.Count).")
    }
    if ($RequireCleanExit -and $summaries.Count -eq 1) {
        $summary = [regex]::Match($summaries[0].Line, $summaryPattern)
        $summaryCallbacks = [uint64]$summary.Groups['callbacks'].Value
        $summaryVisible = [uint64]$summary.Groups['visible'].Value
        $summaryDevices = [uint64]$summary.Groups['devices'].Value
        $summaryUploadSuccesses = [uint64]$summary.Groups['uploadSuccesses'].Value
        $summaryUploadAttempts = [uint64]$summary.Groups['uploadAttempts'].Value
        $summaryUploadFailures = [uint64]$summary.Groups['uploadFailures'].Value
        $summaryRecreateSuccesses = [uint64]$summary.Groups['recreateSuccesses'].Value
        $summaryRecreateStarts = [uint64]$summary.Groups['recreateStarts'].Value
        $summaryRecreateFailures = [uint64]$summary.Groups['recreateFailures'].Value

        if ($summaryVisible -gt $summaryCallbacks) {
            $failures.Add('Renderer summary has more visible frames than callbacks.')
        }
        if ($summaryDevices -ne $devices.Count) {
            $failures.Add('Renderer summary device count does not match the log.')
        }
        if ($summaryUploadSuccesses -ne $uploads.Count) {
            $failures.Add('Renderer summary upload count does not match the log.')
        }
        if (($summaryUploadSuccesses + $summaryUploadFailures) -ne $summaryUploadAttempts) {
            $failures.Add('Renderer summary upload outcomes do not match its attempt count.')
        }
        if ($summaryRecreateSuccesses -ne $recreateSuccesses.Count -or
            $summaryRecreateStarts -ne $recreateStarts.Count) {
            $failures.Add('Renderer summary recreation counts do not match the log.')
        }
        if (($summaryRecreateSuccesses + $summaryRecreateFailures) -ne
            $summaryRecreateStarts) {
            $failures.Add('Renderer summary recreation outcomes do not match its start count.')
        }
    }

    if (($ExpectedWidth -eq 0) -xor ($ExpectedHeight -eq 0)) {
        $failures.Add('ExpectedWidth and ExpectedHeight must be supplied together.')
    } elseif ($ExpectedWidth -gt 0) {
        $expectedPattern = "backbuffer=$($ExpectedWidth)x$($ExpectedHeight)"
        $matchingDevices = @($devices | Where-Object { $_.Line.Contains($expectedPattern) })
        if ($matchingDevices.Count -lt 1) {
            $failures.Add("No validated Direct3D device used backbuffer $($ExpectedWidth)x$($ExpectedHeight).")
        }
    }

    $times = [System.Collections.Generic.List[double]]::new()
    foreach ($match in $uploadTimes) {
        $parsed = [regex]::Match($match.Line, 'took ([0-9]+(?:\.[0-9]+)?) microseconds')
        if ($parsed.Success) {
            $times.Add([double]::Parse($parsed.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture))
        }
    }

    if ($failures.Count -gt 0) {
        Write-Host 'Phase 1 log check failed:'
        foreach ($failure in $failures) { Write-Host "- $failure" }
        exit 1
    }

    $minimumTime = if ($times.Count -gt 0) { ($times | Measure-Object -Minimum).Minimum } else { 0.0 }
    $maximumTime = if ($times.Count -gt 0) { ($times | Measure-Object -Maximum).Maximum } else { 0.0 }
    Write-Host 'Phase 1 log check passed.'
    Write-Host "Validated devices: $($devices.Count)"
    Write-Host "Successful uploads: $($uploads.Count)"
    Write-Host ("Upload time range: {0:F2} to {1:F2} microseconds" -f $minimumTime, $maximumTime)
    Write-Host "Successful recreations: $($recreateSuccesses.Count)"
    if ($RequireCleanExit) {
        Write-Host 'Renderer session summary: present'
        Write-Host 'Clean shutdown record: present'
    }
} catch {
    Write-Host "Phase 1 log check could not run: $($_.Exception.Message)"
    exit 1
}
