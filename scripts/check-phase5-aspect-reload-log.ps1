[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[(ERROR|WARN)\].*$') {
    throw "The Phase 5 aspect and reload log contains a warning or error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The Phase 5 aspect and reload log exposes an absolute path.'
}
if ($log -match '(?im)\b(title|comment|description|artist|album)=') {
    throw 'The Phase 5 aspect and reload log exposes embedded metadata.'
}

$patterns = [ordered]@{
    Fit = 'Configuration accepted: enabled=1 aspect=fit .+ logging=normal'
    Open = 'Playback opened catalog item: session=\d+ bytes=\d+ media=Aspect Mode 4x3\.mp4'
    Stream = 'Playback stream summary: source=160x120 display=160x120 rotation=0 duration_us=30000000 audio=1 source_audio=2ch@48000 output_audio=2ch@48000'
    Fill = 'Configuration accepted: enabled=1 aspect=fill .+ logging=normal'
    Reload = 'Configuration reloaded while playback was idle'
    Shutdown = 'Process shutdown requested'
}

$fit = [Text.RegularExpressions.Regex]::Match($log, $patterns.Fit)
$fill = [Text.RegularExpressions.Regex]::Match($log, $patterns.Fill)
$reload = [Text.RegularExpressions.Regex]::Match($log, $patterns.Reload)
$shutdown = [Text.RegularExpressions.Regex]::Match($log, $patterns.Shutdown)
$opens = [Text.RegularExpressions.Regex]::Matches($log, $patterns.Open)
$streams = [Text.RegularExpressions.Regex]::Matches($log, $patterns.Stream)

foreach ($required in ([ordered]@{
        Fit = $fit
        Fill = $fill
        Reload = $reload
        Shutdown = $shutdown
    }).GetEnumerator()) {
    if (-not $required.Value.Success) {
        throw "The Phase 5 aspect and reload log is missing its $($required.Key) record."
    }
}
if ($opens.Count -ne 2) {
    throw "The Phase 5 aspect and reload log must contain two 4:3 playback starts, found $($opens.Count)."
}
if ($streams.Count -ne 2) {
    throw "The Phase 5 aspect and reload log must contain two 4:3 stream summaries, found $($streams.Count)."
}
if ($fit.Index -ge $opens[0].Index -or
    $opens[0].Index -ge $streams[0].Index -or
    $streams[0].Index -ge $fill.Index -or
    $fill.Index -ge $reload.Index -or
    $reload.Index -ge $opens[1].Index -or
    $opens[1].Index -ge $streams[1].Index -or
    $streams[1].Index -ge $shutdown.Index) {
    throw 'The Fit, reload, Fill, playback, or shutdown records are out of order.'
}

Write-Host 'Phase 5 live aspect and idle reload log passed: fit_starts=1 fill_starts=1 stream_summaries=2'
