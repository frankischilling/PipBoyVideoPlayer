Set-StrictMode -Version Latest

function New-PbvpFocusCycleState {
    [CmdletBinding()]
    param()

    return [pscustomobject]@{
        Started = $false
        FirstForegroundUtc = $null
        IsAway = $false
        AwayStartedUtc = $null
        CompletedCycles = [uint32]0
        TotalAwayMilliseconds = [double]0
        MinimumAwayMilliseconds = $null
        MaximumAwayMilliseconds = $null
    }
}

function Update-PbvpFocusCycleState {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][pscustomobject]$State,
        [Parameter(Mandatory)][bool]$GameIsForeground,
        [Parameter(Mandatory)][datetime]$UtcNow
    )

    if (-not $State.Started) {
        if ($GameIsForeground) {
            $State.Started = $true
            $State.FirstForegroundUtc = $UtcNow
        }
        return
    }

    if (-not $GameIsForeground) {
        if (-not $State.IsAway) {
            $State.IsAway = $true
            $State.AwayStartedUtc = $UtcNow
        }
        return
    }

    if (-not $State.IsAway) {
        return
    }

    $awayMilliseconds = [Math]::Max(
        0.0,
        ($UtcNow - [datetime]$State.AwayStartedUtc).TotalMilliseconds)
    $State.CompletedCycles++
    $State.TotalAwayMilliseconds += $awayMilliseconds
    if ($null -eq $State.MinimumAwayMilliseconds -or
        $awayMilliseconds -lt $State.MinimumAwayMilliseconds) {
        $State.MinimumAwayMilliseconds = $awayMilliseconds
    }
    if ($null -eq $State.MaximumAwayMilliseconds -or
        $awayMilliseconds -gt $State.MaximumAwayMilliseconds) {
        $State.MaximumAwayMilliseconds = $awayMilliseconds
    }
    $State.IsAway = $false
    $State.AwayStartedUtc = $null
}

function Complete-PbvpFocusCycleState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][pscustomobject]$State)

    $average = if ($State.CompletedCycles -gt 0) {
        $State.TotalAwayMilliseconds / $State.CompletedCycles
    } else {
        0.0
    }

    return [ordered]@{
        TrackingStarted = [bool]$State.Started
        FirstForegroundUtc = if ($null -eq $State.FirstForegroundUtc) {
            $null
        } else {
            ([datetime]$State.FirstForegroundUtc).ToString('o')
        }
        CompletedCycles = [uint32]$State.CompletedCycles
        IncompleteFocusLoss = [bool]($State.Started -and $State.IsAway)
        MinimumAwayMilliseconds = if ($null -eq $State.MinimumAwayMilliseconds) {
            0.0
        } else {
            [Math]::Round([double]$State.MinimumAwayMilliseconds, 2)
        }
        AverageAwayMilliseconds = [Math]::Round($average, 2)
        MaximumAwayMilliseconds = if ($null -eq $State.MaximumAwayMilliseconds) {
            0.0
        } else {
            [Math]::Round([double]$State.MaximumAwayMilliseconds, 2)
        }
    }
}

Export-ModuleMember -Function @(
    'New-PbvpFocusCycleState',
    'Update-PbvpFocusCycleState',
    'Complete-PbvpFocusCycleState')
