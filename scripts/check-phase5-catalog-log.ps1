[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[(ERROR|WARN)\].*$') {
    throw "The Phase 5 log contains a warning or error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The Phase 5 log exposes an absolute path.'
}
if ($log -match '(?im)\b(title|comment|description|artist|album)=') {
    throw 'The Phase 5 log exposes embedded metadata.'
}

$requiredPatterns = [ordered]@{
    Runtime = 'Private FFmpeg runtime accepted:'
    Configuration = 'Configuration accepted: enabled=1 .+ catalog=500 display_chars=128 .+ logging=normal'
    Input = 'Scoped MapMenu input bridge attached after vtable validation'
    Catalog = 'Video catalog scan finished: status=ok entries=10 truncated=0 win32=0'
    Open = 'Playback opened catalog item: session=\d+ bytes=49267'
    Surface = 'Decoded BGRA video reached PBVP_VideoSurface'
    Joined = 'Playback audio stopped and decoder worker joined'
    Renderer = 'Renderer summary:'
    Shutdown = 'Process shutdown requested'
}
$positions = [ordered]@{}
foreach ($entry in $requiredPatterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::Match($log, $entry.Value)
    if (-not $match.Success) {
        throw "The Phase 5 log is missing its $($entry.Key) record."
    }
    $positions[$entry.Key] = $match.Index
}

$catalogMatches = [Text.RegularExpressions.Regex]::Matches(
    $log, $requiredPatterns.Catalog)
$openMatches = [Text.RegularExpressions.Regex]::Matches(
    $log, $requiredPatterns.Open)
if ($catalogMatches.Count -lt 2) {
    throw 'The Phase 5 log does not contain two catalog scans.'
}
if ($openMatches.Count -lt 2) {
    throw 'The Phase 5 log does not contain two catalog playback starts.'
}
if ($catalogMatches[1].Index -le $openMatches[0].Index -or
    $openMatches[1].Index -le $catalogMatches[1].Index) {
    throw 'The second catalog scan and playback start are out of order.'
}

$orderedKeys = @($requiredPatterns.Keys)
for ($index = 1; $index -lt $orderedKeys.Count; $index++) {
    if ($positions[$orderedKeys[$index]] -le $positions[$orderedKeys[$index - 1]]) {
        throw 'The Phase 5 lifecycle records are out of order.'
    }
}
Write-Host "Phase 5 live catalog log passed: scans=$($catalogMatches.Count) starts=$($openMatches.Count) entries=10"
