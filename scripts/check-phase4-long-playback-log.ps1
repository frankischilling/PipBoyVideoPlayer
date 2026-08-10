[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[ERROR\].*$') {
    throw "The long playback log contains an error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The long playback log exposes an absolute path.'
}

$requiredPatterns = [ordered]@{
    Runtime = 'Private FFmpeg runtime accepted:'
    Armed = 'PBVP_PLAYBACK_LONG_TEST_ARMED: opening the generated 30-minute 720p30 fixture at volume 0\.03'
    Surface = 'Decoded BGRA video reached PBVP_VideoSurface'
    Passed = 'Integrated playback long test passed: decoded=(?<decoded>\d+) presented=(?<presented>\d+) dropped=(?<dropped>\d+) audio_samples=(?<samples>\d+) clock_us=(?<clock>\d+) video_end_us=(?<videoEnd>\d+) sync_error_us=(?<syncError>\d+) underruns=0 seeks=0 video_submitted=(?<submitted>\d+) video_uploaded=(?<uploaded>\d+) mailbox_replaced=(?<replaced>\d+) upload_us=(?<minimum>[\d.]+)/(?<average>[\d.]+)/(?<maximum>[\d.]+) private_delta=(?<private>\d+) staged_peak=(?<staged>\d+) decoder_video_peak=(?<decoderVideo>\d+) decoder_audio_peak=(?<decoderAudio>\d+) generation=1'
    Joined = 'Playback audio stopped and decoder worker joined'
    Renderer = 'Renderer summary:'
    Shutdown = 'Process shutdown requested'
}
$positions = [ordered]@{}
foreach ($entry in $requiredPatterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::Match($log, $entry.Value)
    if (-not $match.Success) {
        throw "The long playback log is missing its $($entry.Key) record."
    }
    $positions[$entry.Key] = $match.Index
    if ($entry.Key -eq 'Passed') {
        $decoded = [uint64]$match.Groups['decoded'].Value
        $presented = [uint64]$match.Groups['presented'].Value
        $dropped = [uint64]$match.Groups['dropped'].Value
        $samples = [uint64]$match.Groups['samples'].Value
        $clock = [int64]$match.Groups['clock'].Value
        $videoEnd = [int64]$match.Groups['videoEnd'].Value
        $syncError = [int64]$match.Groups['syncError'].Value
        $submitted = [uint64]$match.Groups['submitted'].Value
        $uploaded = [uint64]$match.Groups['uploaded'].Value
        $replaced = [uint64]$match.Groups['replaced'].Value
        $minimum = [double]$match.Groups['minimum'].Value
        $average = [double]$match.Groups['average'].Value
        $maximum = [double]$match.Groups['maximum'].Value
        $private = [uint64]$match.Groups['private'].Value
        $staged = [uint64]$match.Groups['staged'].Value
        $decoderVideo = [uint64]$match.Groups['decoderVideo'].Value
        $decoderAudio = [uint64]$match.Groups['decoderAudio'].Value
        if ($decoded -lt 53900 -or $decoded -gt 54100 -or
            $presented -eq 0 -or $presented + $dropped -gt $decoded -or
            $samples -lt 86400000 -or $samples -gt 86410000 -or
            $clock -lt 1800000000 -or $clock -gt 1800100000 -or
            $videoEnd -lt 1799900000 -or $videoEnd -gt 1800050000 -or
            $syncError -gt 50000 -or $submitted -eq 0 -or $uploaded -eq 0 -or
            $uploaded -gt $submitted -or $replaced -gt $submitted -or
            $minimum -le 0.0 -or $average -lt $minimum -or
            $maximum -lt $average -or $maximum -ge 10000.0 -or
            $private -ge 134217728 -or $staged -gt 8388608 -or
            $decoderVideo -gt 33554432 -or $decoderAudio -gt 4194304) {
            throw 'The long playback output is outside the acceptance limits.'
        }
    }
}

$progress = [Text.RegularExpressions.Regex]::Matches(
    $log,
    'Integrated playback long test progress: clock_us=(?<clock>\d+) decoded=\d+ presented=\d+ dropped=\d+ underruns=0 private_delta=(?<private>\d+)')
if ($progress.Count -lt 5) {
    throw 'The long playback log has fewer than five progress records.'
}
$previousClock = 0L
foreach ($entry in $progress) {
    $progressClock = [int64]$entry.Groups['clock'].Value
    $progressPrivate = [uint64]$entry.Groups['private'].Value
    if ($progressClock -le $previousClock -or $progressPrivate -ge 134217728) {
        throw 'The long playback progress records are invalid.'
    }
    $previousClock = $progressClock
}

$orderedKeys = @($requiredPatterns.Keys)
for ($index = 1; $index -lt $orderedKeys.Count; $index++) {
    if ($positions[$orderedKeys[$index]] -le $positions[$orderedKeys[$index - 1]]) {
        throw 'The long playback lifecycle records are out of order.'
    }
}
Write-Host "Phase 4 long playback log passed: decoded=$decoded presented=$presented dropped=$dropped samples=$samples clock_us=$clock sync_error_us=$syncError private_delta=$private upload_max_us=$maximum"
