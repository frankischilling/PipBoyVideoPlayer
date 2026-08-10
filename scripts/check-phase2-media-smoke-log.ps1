[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[ERROR\].*$') {
    throw "The media smoke log contains an error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The media smoke log exposes an absolute path.'
}

$requiredPatterns = [ordered]@{
    Runtime = 'Private FFmpeg runtime accepted:'
    Armed = 'PBVP_MEDIA_SMOKE_TEST_ARMED: waiting for a stable main-menu memory baseline'
    ControlStarted = 'Media smoke no-decode memory control started after five-second settle delay'
    ControlPassed = 'Media smoke no-decode control passed: private_delta=(?<control>\d+)'
    Started = 'Media smoke decoder worker started for PBVP-Phase2-Smoke\.mp4'
    Passed = 'Media smoke passed: source=1920x1080 video=30 audio_chunks=(?<chunks>\d+) audio_samples=(?<samples>\d+) private_delta=(?<private>\d+) video_queue_peak=(?<video>\d+) audio_queue_peak=(?<audio>\d+) generation=1'
    Joined = 'Media smoke worker joined before private FFmpeg unload'
    Shutdown = 'Process shutdown requested'
}
$positions = [ordered]@{}
foreach ($entry in $requiredPatterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::Match($log, $entry.Value)
    if (-not $match.Success) {
        throw "The media smoke log is missing its $($entry.Key) record."
    }
    $positions[$entry.Key] = $match.Index
    if ($entry.Key -eq 'Passed') {
        $chunks = [uint64]$match.Groups['chunks'].Value
        $samples = [uint64]$match.Groups['samples'].Value
        $private = [uint64]$match.Groups['private'].Value
        $video = [uint64]$match.Groups['video'].Value
        $audio = [uint64]$match.Groups['audio'].Value
        if ($chunks -eq 0 -or $samples -lt 47000 -or $samples -gt 49000 -or
            $private -ge 134217728 -or $video -gt 33554432 -or $audio -gt 4194304) {
            throw 'The media smoke output counts are outside the expected range.'
        }
    } elseif ($entry.Key -eq 'ControlPassed') {
        $control = [uint64]$match.Groups['control'].Value
        if ($control -ge 33554432) {
            throw 'The media smoke no-decode control is outside the stable range.'
        }
    }
}

$orderedKeys = @($requiredPatterns.Keys)
for ($index = 1; $index -lt $orderedKeys.Count; $index++) {
    if ($positions[$orderedKeys[$index]] -le $positions[$orderedKeys[$index - 1]]) {
        throw 'The media smoke lifecycle records are out of order.'
    }
}
Write-Host "Phase 2 live media smoke log passed: control_delta=$control audio_chunks=$chunks audio_samples=$samples private_delta=$private video_queue_peak=$video audio_queue_peak=$audio"
