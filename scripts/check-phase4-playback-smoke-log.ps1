[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[ERROR\].*$') {
    throw "The playback smoke log contains an error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The playback smoke log exposes an absolute path.'
}

$requiredPatterns = [ordered]@{
    Runtime = 'Private FFmpeg runtime accepted:'
    Armed = 'PBVP_PLAYBACK_SMOKE_TEST_ARMED: opening the generated Phase 4 fixture at volume 0\.10'
    Surface = 'Decoded BGRA video reached PBVP_VideoSurface'
    Passed = 'Integrated playback smoke passed: decoded=(?<decoded>\d+) presented=(?<presented>\d+) dropped=(?<dropped>\d+) audio_samples=(?<samples>\d+) clock_us=(?<clock>\d+) underruns=0 seeks=0 video_submitted=(?<submitted>\d+) video_uploaded=(?<uploaded>\d+) mailbox_replaced=(?<replaced>\d+) upload_us=(?<minimum>[\d.]+)/(?<average>[\d.]+)/(?<maximum>[\d.]+) generation=1'
    Joined = 'Playback audio stopped and decoder worker joined'
    Renderer = 'Renderer summary:'
    Shutdown = 'Process shutdown requested'
}
$positions = [ordered]@{}
foreach ($entry in $requiredPatterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::Match($log, $entry.Value)
    if (-not $match.Success) {
        throw "The playback smoke log is missing its $($entry.Key) record."
    }
    $positions[$entry.Key] = $match.Index
    if ($entry.Key -eq 'Passed') {
        $decoded = [uint64]$match.Groups['decoded'].Value
        $presented = [uint64]$match.Groups['presented'].Value
        $dropped = [uint64]$match.Groups['dropped'].Value
        $samples = [uint64]$match.Groups['samples'].Value
        $clock = [uint64]$match.Groups['clock'].Value
        $submitted = [uint64]$match.Groups['submitted'].Value
        $uploaded = [uint64]$match.Groups['uploaded'].Value
        $replaced = [uint64]$match.Groups['replaced'].Value
        $minimum = [double]$match.Groups['minimum'].Value
        $average = [double]$match.Groups['average'].Value
        $maximum = [double]$match.Groups['maximum'].Value
        if ($decoded -ne 20 -or $presented -eq 0 -or
            $presented + $dropped -gt $decoded -or
            $samples -ne 96967 -or $clock -ne 2020125 -or
            $submitted -eq 0 -or $uploaded -eq 0 -or
            $uploaded -gt $submitted -or $replaced -gt $submitted -or
            $minimum -le 0.0 -or $average -lt $minimum -or
            $maximum -lt $average -or $maximum -ge 10000.0) {
            throw 'The playback smoke output is outside the expected limits.'
        }
    }
}

$orderedKeys = @($requiredPatterns.Keys)
for ($index = 1; $index -lt $orderedKeys.Count; $index++) {
    if ($positions[$orderedKeys[$index]] -le $positions[$orderedKeys[$index - 1]]) {
        throw 'The playback smoke lifecycle records are out of order.'
    }
}
Write-Host "Phase 4 live playback log passed: decoded=$decoded presented=$presented dropped=$dropped samples=$samples clock_us=$clock video_uploaded=$uploaded upload_max_us=$maximum"
