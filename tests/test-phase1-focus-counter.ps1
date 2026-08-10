[CmdletBinding()]
param([Parameter(Mandatory)][string]$CounterModule)

$ErrorActionPreference = 'Stop'
Import-Module (Resolve-Path -LiteralPath $CounterModule).Path -Force

$state = New-PbvpFocusCycleState
$start = [DateTime]::Parse('2026-08-10T12:00:00.0000000Z').ToUniversalTime()

Update-PbvpFocusCycleState -State $state -GameIsForeground $false -UtcNow $start
if ($state.Started -or $state.CompletedCycles -ne 0) {
    throw 'A background sample before the first game foreground started tracking.'
}

Update-PbvpFocusCycleState -State $state -GameIsForeground $true `
    -UtcNow $start.AddSeconds(1)
Update-PbvpFocusCycleState -State $state -GameIsForeground $true `
    -UtcNow $start.AddSeconds(2)
Update-PbvpFocusCycleState -State $state -GameIsForeground $false `
    -UtcNow $start.AddSeconds(3)
Update-PbvpFocusCycleState -State $state -GameIsForeground $false `
    -UtcNow $start.AddSeconds(4)
Update-PbvpFocusCycleState -State $state -GameIsForeground $true `
    -UtcNow $start.AddSeconds(5)
Update-PbvpFocusCycleState -State $state -GameIsForeground $false `
    -UtcNow $start.AddSeconds(6)
Update-PbvpFocusCycleState -State $state -GameIsForeground $true `
    -UtcNow $start.AddMilliseconds(6500)

$summary = Complete-PbvpFocusCycleState -State $state
if ($summary.CompletedCycles -ne 2 -or
    $summary.IncompleteFocusLoss -or
    $summary.MinimumAwayMilliseconds -ne 500.0 -or
    $summary.AverageAwayMilliseconds -ne 1250.0 -or
    $summary.MaximumAwayMilliseconds -ne 2000.0) {
    throw 'The completed focus-cycle summary is incorrect.'
}

Update-PbvpFocusCycleState -State $state -GameIsForeground $false `
    -UtcNow $start.AddSeconds(8)
$summary = Complete-PbvpFocusCycleState -State $state
if ($summary.CompletedCycles -ne 2 -or -not $summary.IncompleteFocusLoss) {
    throw 'An incomplete final focus loss was counted as a completed cycle.'
}

Write-Host 'Phase 1 focus-cycle counter tests passed.'
