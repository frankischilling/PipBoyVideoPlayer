[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputPath,
    [ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$ProcessName = 'FalloutNV',
    [ValidateRange(1, 10000)][uint32]$TargetCycles = 50,
    [ValidateRange(10, 5000)][int]$PollIntervalMilliseconds = 100,
    [ValidateRange(1, 3600)][int]$WaitTimeoutSeconds = 300,
    [switch]$StopAtTarget
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'phase1-focus-counter.psm1') -Force

if (-not ('PbvpForegroundWindow' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class PbvpForegroundWindow
{
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(
        IntPtr window,
        out uint processId);
}
'@
}

$output = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $output
if (-not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
    throw "The focus-cycle output directory does not exist: $outputDirectory"
}

Write-Host "Waiting for $ProcessName..."
$wait = [Diagnostics.Stopwatch]::StartNew()
$game = $null
while ($null -eq $game -and $wait.Elapsed.TotalSeconds -lt $WaitTimeoutSeconds) {
    $matches = @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)
    if ($matches.Count -gt 1) {
        throw "More than one $ProcessName process is running."
    }
    if ($matches.Count -eq 1) {
        $game = $matches[0]
        break
    }
    Start-Sleep -Milliseconds $PollIntervalMilliseconds
}
if ($null -eq $game) {
    throw "Timed out waiting for $ProcessName."
}

$state = New-PbvpFocusCycleState
$lastReportedCount = [uint32]0
$sampleCount = [uint64]0
$endReason = 'process-exit'
Write-Host "Tracking foreground returns for process $($game.Id)."
Write-Host "Target: $TargetCycles completed focus cycles."

while ($true) {
    try {
        $game.Refresh()
        if ($game.HasExited) {
            break
        }
    } catch {
        break
    }

    $foregroundProcessId = [uint32]0
    $foregroundWindow = [PbvpForegroundWindow]::GetForegroundWindow()
    if ($foregroundWindow -ne [IntPtr]::Zero) {
        [void][PbvpForegroundWindow]::GetWindowThreadProcessId(
            $foregroundWindow,
            [ref]$foregroundProcessId)
    }

    Update-PbvpFocusCycleState -State $state `
        -GameIsForeground ($foregroundProcessId -eq [uint32]$game.Id) `
        -UtcNow ([DateTime]::UtcNow)
    $sampleCount++

    if ($state.CompletedCycles -ne $lastReportedCount) {
        $lastReportedCount = $state.CompletedCycles
        Write-Host "Completed focus cycles: $lastReportedCount / $TargetCycles"
    }
    if ($StopAtTarget -and $state.CompletedCycles -ge $TargetCycles) {
        $endReason = 'target-reached'
        break
    }

    Start-Sleep -Milliseconds $PollIntervalMilliseconds
}

$cycleSummary = Complete-PbvpFocusCycleState -State $state
$result = [ordered]@{
    SchemaVersion = 1
    ProcessName = $ProcessName
    ProcessId = [uint32]$game.Id
    TargetCycles = $TargetCycles
    TargetMet = [bool]($state.CompletedCycles -ge $TargetCycles)
    EndReason = $endReason
    PollIntervalMilliseconds = $PollIntervalMilliseconds
    SampleCount = $sampleCount
    EndedUtc = [DateTime]::UtcNow.ToString('o')
}
foreach ($entry in $cycleSummary.GetEnumerator()) {
    $result[$entry.Key] = $entry.Value
}

$temporary = "$output.pbvp-$([Guid]::NewGuid().ToString('N')).tmp"
try {
    [IO.File]::WriteAllText(
        $temporary,
        ($result | ConvertTo-Json) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $output -Force
} finally {
    if (Test-Path -LiteralPath $temporary) {
        Remove-Item -LiteralPath $temporary -Force
    }
}

Write-Host "Focus-cycle result written to $output"
Write-Host "Completed cycles: $($state.CompletedCycles)"
if (-not $result.TargetMet) {
    exit 2
}
