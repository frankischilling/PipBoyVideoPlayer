[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[(ERROR|WARN)\].*$') {
    throw "The Phase 5 controller log contains a warning or error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The Phase 5 controller log exposes an absolute path.'
}
if ($log -match '(?im)\b(title|comment|description|artist|album)=') {
    throw 'The Phase 5 controller log exposes embedded metadata.'
}

$patterns = [ordered]@{
    Input = 'Scoped MapMenu input bridge attached after vtable validation'
    Catalog = 'Video catalog scan finished: status=ok entries=(?<entries>\d+) truncated=0 win32=0'
    Controller = 'Videos input method changed: controller'
    Open = 'Playback opened catalog item: session=\d+ bytes=\d+ media=[^\r\n]+'
    Stream = 'Playback stream summary: source=\d+x\d+ display=\d+x\d+ rotation=\d+ duration_us=\d+ audio=[01] source_audio=\d+ch@\d+ output_audio=\d+ch@\d+'
    KeyboardMouse = 'Videos input method changed: keyboard-mouse'
    Shutdown = 'Process shutdown requested'
}
$matches = [ordered]@{}
$nextIndex = 0
foreach ($entry in $patterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::new($entry.Value).Match(
        $log, $nextIndex)
    if (-not $match.Success) {
        throw "The Phase 5 controller log is missing its ordered $($entry.Key) record."
    }
    $matches[$entry.Key] = $match
    $nextIndex = $match.Index + $match.Length
}

$entries = 0
if (-not [int]::TryParse($matches.Catalog.Groups['entries'].Value, [ref]$entries) -or
    $entries -lt 1 -or $entries -gt 500) {
    throw 'The Phase 5 controller log has an invalid catalog size.'
}
Write-Host "Phase 5 live controller log passed: entries=$entries controller_playback=1 keyboard_mouse_return=1"
