[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BaseLog,
    [Parameter(Mandatory)][string]$VuiPlusLog,
    [Parameter(Mandatory)][string]$CleanVanillaHudLog,
    [Parameter(Mandatory)][string]$ExtendedLog
)

$ErrorActionPreference = 'Stop'
$profiles = [ordered]@{
    Base = $BaseLog
    VuiPlus = $VuiPlusLog
    CleanVanillaHud = $CleanVanillaHudLog
    Extended = $ExtendedLog
}
$resolvedPaths = @{}
$resolvedProfiles = [ordered]@{}
foreach ($entry in $profiles.GetEnumerator()) {
    $path = (Resolve-Path -LiteralPath $entry.Value).Path
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "The Phase 5 $($entry.Key) log is not a file."
    }
    $pathKey = $path.ToUpperInvariant()
    if ($resolvedPaths.ContainsKey($pathKey)) {
        throw 'Each Phase 5 UI profile requires its own preserved log.'
    }
    $resolvedPaths[$pathKey] = $true
    $resolvedProfiles[$entry.Key] = $path
}

function Test-ProfileLog {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$Path
    )
    $log = Get-Content -LiteralPath $Path -Raw
    if ($log -match '(?im)^.*\[(ERROR|WARN)\].*$') {
        throw "The Phase 5 $Label log contains a warning or error: $($Matches[0])"
    }
    if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
        throw "The Phase 5 $Label log exposes an absolute path."
    }
    if ($log -match '(?im)\b(title|comment|description|artist|album)=') {
        throw "The Phase 5 $Label log exposes embedded metadata."
    }

    $patterns = [ordered]@{
        Configuration = 'Configuration accepted: enabled=1 .+ logging=normal'
        Runtime = 'Private FFmpeg runtime accepted:'
        Input = 'Scoped MapMenu input bridge attached after vtable validation'
        Catalog = 'Video catalog scan finished: status=ok entries=(?<entries>\d+) truncated=0 win32=0'
        Open = 'Playback opened catalog item: session=\d+ bytes=\d+ media=[^\r\n]+'
        Stream = 'Playback stream summary: source=\d+x\d+ display=\d+x\d+ rotation=\d+ duration_us=\d+ audio=[01] source_audio=\d+ch@\d+ output_audio=\d+ch@\d+'
        Surface = 'Decoded BGRA video reached PBVP_VideoSurface'
        Joined = 'Playback audio stopped and decoder worker joined'
        Renderer = 'Renderer summary: callbacks=\d+ visible=\d+ devices=\d+ video-submitted=\d+ video-uploaded=\d+ mailbox-replaced=\d+ mailbox-cleared=\d+ upload-successes=(?<uploads>\d+) upload-attempts=(?<attempts>\d+) upload-failures=0'
        Shutdown = 'Process shutdown requested'
    }
    $matches = [ordered]@{}
    $nextIndex = 0
    foreach ($entry in $patterns.GetEnumerator()) {
        $match = [Text.RegularExpressions.Regex]::new($entry.Value).Match(
            $log, $nextIndex)
        if (-not $match.Success) {
            throw "The Phase 5 $Label log is missing its ordered $($entry.Key) record."
        }
        $matches[$entry.Key] = $match
        $nextIndex = $match.Index + $match.Length
    }

    $entries = 0
    $uploads = 0
    $attempts = 0
    if (-not [int]::TryParse($matches.Catalog.Groups['entries'].Value, [ref]$entries) -or
        $entries -lt 1 -or $entries -gt 500) {
        throw "The Phase 5 $Label log has an invalid catalog size."
    }
    if (-not [int]::TryParse($matches.Renderer.Groups['uploads'].Value, [ref]$uploads) -or
        -not [int]::TryParse($matches.Renderer.Groups['attempts'].Value, [ref]$attempts) -or
        $uploads -lt 1 -or $attempts -lt $uploads) {
        throw "The Phase 5 $Label log has invalid renderer upload accounting."
    }
    Write-Host "Phase 5 $Label UI log passed: entries=$entries uploads=$uploads attempts=$attempts"
}

foreach ($entry in $resolvedProfiles.GetEnumerator()) {
    Test-ProfileLog -Label $entry.Key -Path $entry.Value
}
Write-Host 'Phase 5 four-profile UI matrix logs passed.'
