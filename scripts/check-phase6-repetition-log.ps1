[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath,
    [ValidateRange(1, 10000)][int]$MinimumPlaybackSessions = 100,
    [ValidateRange(0, 10000)][int]$MinimumForwardSeeks = 20,
    [ValidateRange(0, 10000)][int]$MinimumBackwardSeeks = 20
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[(WARN|ERROR)\].*$') {
    throw "The repetition log contains a warning or error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The repetition log exposes an absolute path.'
}

$requiredPatterns = [ordered]@{
    Startup = 'Pip-Boy Video Player [\d.]+ loading;'
    Runtime = 'Private FFmpeg runtime accepted:'
    Catalog = 'Video catalog scan finished: status=ok entries=\d+'
    Surface = 'Decoded BGRA video reached PBVP_VideoSurface'
    Joined = 'Playback audio stopped and decoder worker joined'
    Renderer = 'Renderer summary: callbacks=(?<callbacks>\d+) visible=(?<visible>\d+) devices=(?<devices>\d+) video-submitted=(?<submitted>\d+) video-uploaded=(?<uploaded>\d+) mailbox-replaced=(?<replaced>\d+) mailbox-cleared=(?<cleared>\d+) upload-successes=(?<successes>\d+) upload-attempts=(?<attempts>\d+) upload-failures=(?<failures>\d+) upload-us=(?<minimum>[\d.]+)/(?<average>[\d.]+)/(?<maximum>[\d.]+)'
    Shutdown = 'Process shutdown requested'
}
$positions = [ordered]@{}
$rendererMatch = $null
foreach ($entry in $requiredPatterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::Match($log, $entry.Value)
    if (-not $match.Success) {
        throw "The repetition log is missing its $($entry.Key) record."
    }
    $positions[$entry.Key] = $match.Index
    if ($entry.Key -eq 'Renderer') {
        $rendererMatch = $match
    }
}

$openMatches = [Text.RegularExpressions.Regex]::Matches(
    $log,
    'Playback opened catalog item: session=\d+ bytes=\d+ media=[^\r\n]+')
$summaryPattern = 'Playback session summary: playback=(?<playback>\d+) terminal=(?<terminal>[a-z_]+) state=(?<state>[a-z_]+) error=(?<error>[a-z_]+) failure_site=(?<failure>[a-z_]+) generation=(?<generation>\d+) decoded=(?<decoded>\d+) presented=(?<presented>\d+) dropped=(?<dropped>\d+) stale_video=(?<staleVideo>\d+) audio_samples=(?<audioSamples>\d+) stale_audio=(?<staleAudio>\d+) clock_us=(?<clock>\d+) video_end_us=(?<videoEnd>\d+) underruns=(?<underruns>\d+) seeks=(?<seeks>\d+) forward_seeks=(?<forward>\d+) backward_seeks=(?<backward>\d+) pauses=(?<pauses>\d+) resumes=(?<resumes>\d+) buffering=(?<buffering>\d+) max_update_gap_ms=(?<gap>\d+) staged_peak=(?<staged>\d+) decoder_video_peak=(?<decoderVideo>\d+) decoder_audio_peak=(?<decoderAudio>\d+)'
$summaries = [Text.RegularExpressions.Regex]::Matches($log, $summaryPattern)
if ($summaries.Count -lt $MinimumPlaybackSessions -or
    $openMatches.Count -ne $summaries.Count) {
    throw 'The repetition log does not contain the required matched playback sessions.'
}

$totalForward = 0L
$totalBackward = 0L
$previousPlayback = 0L
foreach ($summary in $summaries) {
    $playback = [Int64]$summary.Groups['playback'].Value
    $generation = [Int64]$summary.Groups['generation'].Value
    $decoded = [Int64]$summary.Groups['decoded'].Value
    $presented = [Int64]$summary.Groups['presented'].Value
    $dropped = [Int64]$summary.Groups['dropped'].Value
    $audioSamples = [Int64]$summary.Groups['audioSamples'].Value
    $underruns = [Int64]$summary.Groups['underruns'].Value
    $seeks = [Int64]$summary.Groups['seeks'].Value
    $forward = [Int64]$summary.Groups['forward'].Value
    $backward = [Int64]$summary.Groups['backward'].Value
    $buffering = [Int64]$summary.Groups['buffering'].Value
    $staged = [Int64]$summary.Groups['staged'].Value
    $decoderVideo = [Int64]$summary.Groups['decoderVideo'].Value
    $decoderAudio = [Int64]$summary.Groups['decoderAudio'].Value
    if ($playback -ne $previousPlayback + 1) {
        throw 'The repetition log contains a missing or duplicate playback sequence.'
    }
    if ($summary.Groups['terminal'].Value -notmatch
            '^(stopped|completed|presentation_hidden|lifecycle_transition|shutdown)$' -or
        $summary.Groups['state'].Value -ne 'idle' -or
        $summary.Groups['error'].Value -ne 'none' -or
        $summary.Groups['failure'].Value -ne 'none' -or
        $decoded -le 0 -or $presented -le 0 -or
        $presented + $dropped -gt $decoded -or
        $audioSamples -le 0 -or $underruns -ne 0 -or
        $seeks -ne $forward + $backward -or
        $generation -ne $seeks + 1 -or $buffering -le 0 -or
        $staged -gt 8388608 -or $decoderVideo -gt 33554432 -or
        $decoderAudio -gt 4194304) {
        throw "Playback session $playback is outside the repetition limits."
    }
    $totalForward += $forward
    $totalBackward += $backward
    $previousPlayback = $playback
}
if ($totalForward -lt $MinimumForwardSeeks -or
    $totalBackward -lt $MinimumBackwardSeeks) {
    throw 'The repetition log does not contain enough accepted seeks in both directions.'
}

$submitted = [Int64]$rendererMatch.Groups['submitted'].Value
$uploaded = [Int64]$rendererMatch.Groups['uploaded'].Value
$replaced = [Int64]$rendererMatch.Groups['replaced'].Value
$cleared = [Int64]$rendererMatch.Groups['cleared'].Value
$successes = [Int64]$rendererMatch.Groups['successes'].Value
$attempts = [Int64]$rendererMatch.Groups['attempts'].Value
$failures = [Int64]$rendererMatch.Groups['failures'].Value
$minimum = [double]$rendererMatch.Groups['minimum'].Value
$average = [double]$rendererMatch.Groups['average'].Value
$maximum = [double]$rendererMatch.Groups['maximum'].Value
if ($submitted -le 0 -or $uploaded -le 0 -or
    $submitted -ne $uploaded + $replaced + $cleared -or
    $successes -ne $uploaded -or $attempts -ne $successes -or
    $failures -ne 0 -or $minimum -le 0.0 -or
    $average -lt $minimum -or $maximum -lt $average -or
    $maximum -ge 1000.0) {
    throw 'The repetition renderer metrics are outside the acceptance limits.'
}

if ($positions.Startup -ge $positions.Runtime -or
    $positions.Runtime -ge $positions.Catalog -or
    $positions.Catalog -ge $positions.Surface -or
    $summaries[0].Index -le $positions.Surface -or
    $summaries[$summaries.Count - 1].Index -ge $positions.Joined -or
    $positions.Joined -ge $positions.Renderer -or
    $positions.Renderer -ge $positions.Shutdown) {
    throw 'The repetition lifecycle records are out of order.'
}

Write-Host (
    ('Phase 6 repetition log passed: sessions={0} forward_seeks={1} ' +
    'backward_seeks={2} uploads={3} upload_max_us={4}') -f
    $summaries.Count,
    $totalForward,
    $totalBackward,
    $uploaded,
    $maximum)
