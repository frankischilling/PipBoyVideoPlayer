[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[ERROR\].*$') {
    throw "The low-FPS playback log contains an error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The low-FPS playback log exposes an absolute path.'
}

$requiredPatterns = [ordered]@{
    Runtime = 'Private FFmpeg runtime accepted:'
    Armed = 'PBVP_PLAYBACK_LONG_TEST_ARMED: opening the generated 30-minute 720p30 fixture at volume 0\.03'
    Surface = 'Decoded BGRA video reached PBVP_VideoSurface'
    Progress = 'Integrated playback long test progress:'
    Joined = 'Playback audio stopped and decoder worker joined'
    Renderer = 'Renderer summary:'
    Cadence = 'Visible cadence summary:'
    Shutdown = 'Process shutdown requested'
}
$positions = [ordered]@{}
foreach ($entry in $requiredPatterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::Match($log, $entry.Value)
    if (-not $match.Success) {
        throw "The low-FPS playback log is missing its $($entry.Key) record."
    }
    $positions[$entry.Key] = $match.Index
}

$progress = [Text.RegularExpressions.Regex]::Match(
    $log,
    'Integrated playback long test progress: clock_us=(?<clock>\d+) decoded=(?<decoded>\d+) presented=(?<presented>\d+) dropped=(?<dropped>\d+) underruns=(?<underruns>\d+) private_delta=(?<private>\d+)')
if (-not $progress.Success) {
    throw 'The low-FPS progress record could not be parsed.'
}
$clock = [int64]$progress.Groups['clock'].Value
$decoded = [uint64]$progress.Groups['decoded'].Value
$presented = [uint64]$progress.Groups['presented'].Value
$dropped = [uint64]$progress.Groups['dropped'].Value
$underruns = [uint64]$progress.Groups['underruns'].Value
$private = [uint64]$progress.Groups['private'].Value
if ($clock -lt 299000000 -or $clock -gt 301000000 -or
    $decoded -lt 8900 -or $decoded -gt 9100 -or
    $presented -lt 2500 -or $presented -gt 3500 -or
    $dropped -lt 5000 -or $presented + $dropped -gt $decoded -or
    $decoded - ($presented + $dropped) -gt 30 -or
    $underruns -ne 0 -or $private -ge 134217728) {
    throw 'The low-FPS progress record is outside the acceptance limits.'
}

$renderer = [Text.RegularExpressions.Regex]::Match(
    $log,
    'Renderer summary:.*video-submitted=(?<submitted>\d+) video-uploaded=(?<uploaded>\d+) mailbox-replaced=(?<replaced>\d+).*upload-failures=(?<failures>\d+) upload-us=(?<minimum>[\d.]+)/(?<average>[\d.]+)/(?<maximum>[\d.]+)')
if (-not $renderer.Success) {
    throw 'The low-FPS renderer summary could not be parsed.'
}
$submitted = [uint64]$renderer.Groups['submitted'].Value
$uploaded = [uint64]$renderer.Groups['uploaded'].Value
$replaced = [uint64]$renderer.Groups['replaced'].Value
$failures = [uint64]$renderer.Groups['failures'].Value
$minimum = [double]$renderer.Groups['minimum'].Value
$average = [double]$renderer.Groups['average'].Value
$maximum = [double]$renderer.Groups['maximum'].Value
if ($submitted -lt 2500 -or $submitted -gt 3500 -or
    $uploaded -ne $submitted -or $replaced -ne 0 -or $failures -ne 0 -or
    $minimum -le 0.0 -or $average -lt $minimum -or
    $maximum -lt $average -or $maximum -ge 10000.0) {
    throw 'The low-FPS renderer summary is outside the acceptance limits.'
}

$cadence = [Text.RegularExpressions.Regex]::Match(
    $log,
    'Visible cadence summary: samples=(?<samples>\d+) fps=(?<minimum>[\d.]+)/(?<average>[\d.]+)/(?<maximum>[\d.]+)')
if (-not $cadence.Success) {
    throw 'The low-FPS cadence summary could not be parsed.'
}
$cadenceSamples = [uint64]$cadence.Groups['samples'].Value
$cadenceMinimum = [double]$cadence.Groups['minimum'].Value
$cadenceAverage = [double]$cadence.Groups['average'].Value
$cadenceMaximum = [double]$cadence.Groups['maximum'].Value
if ($cadenceSamples -ne 8 -or $cadenceMinimum -lt 8.0 -or
    $cadenceMaximum -gt 12.0 -or $cadenceAverage -lt $cadenceMinimum -or
    $cadenceMaximum -lt $cadenceAverage) {
    throw 'The low-FPS cadence is outside the 10 FPS acceptance range.'
}

$orderedKeys = @($requiredPatterns.Keys)
for ($index = 1; $index -lt $orderedKeys.Count; $index++) {
    if ($positions[$orderedKeys[$index]] -le $positions[$orderedKeys[$index - 1]]) {
        throw 'The low-FPS playback lifecycle records are out of order.'
    }
}

Write-Host "Phase 4 low-FPS playback log passed: clock_us=$clock decoded=$decoded presented=$presented dropped=$dropped underruns=$underruns private_delta=$private upload_max_us=$maximum cadence_fps=$cadenceAverage"
