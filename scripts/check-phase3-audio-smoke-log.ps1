[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath
)

$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -match '(?im)^.*\[ERROR\].*$') {
    throw "The audio smoke log contains an error: $($Matches[0])"
}
if ($log -match '(?i)[A-Z]:\\|file://|file:\\') {
    throw 'The audio smoke log exposes an absolute path.'
}

$requiredPatterns = [ordered]@{
    Runtime = 'Private FFmpeg runtime accepted:'
    Armed = 'PBVP_AUDIO_SMOKE_TEST_ARMED: playing the generated Phase 3 tone at volume 0\.10'
    Started = 'Audio smoke playback started: prebuffer_ms=200 pool_bytes=(?<startPool>\d+)'
    Passed = 'Audio smoke passed: source_rate=44100 source_channels=2 output_rate=48000 output_channels=2 samples=(?<samples>\d+) clock_us=(?<clock>\d+) underruns=0 completions=(?<completed>\d+) stream_ends=(?<ends>\d+) pool_bytes=(?<pool>\d+) generation=1'
    Joined = 'Audio smoke decoder worker joined before private FFmpeg unload'
    Released = 'Audio smoke voices and callback targets released'
    Shutdown = 'Process shutdown requested'
}
$positions = [ordered]@{}
foreach ($entry in $requiredPatterns.GetEnumerator()) {
    $match = [Text.RegularExpressions.Regex]::Match($log, $entry.Value)
    if (-not $match.Success) {
        throw "The audio smoke log is missing its $($entry.Key) record."
    }
    $positions[$entry.Key] = $match.Index
    if ($entry.Key -eq 'Started') {
        $startPool = [uint64]$match.Groups['startPool'].Value
        if ($startPool -ne 262144) {
            throw 'The audio smoke start used an unexpected pool size.'
        }
    } elseif ($entry.Key -eq 'Passed') {
        $samples = [uint64]$match.Groups['samples'].Value
        $clock = [uint64]$match.Groups['clock'].Value
        $completed = [uint64]$match.Groups['completed'].Value
        $ends = [uint64]$match.Groups['ends'].Value
        $pool = [uint64]$match.Groups['pool'].Value
        if ($samples -lt 96000 -or $samples -gt 98000 -or
            $clock -ne 2020125 -or $completed -eq 0 -or
            $ends -ne 1 -or $pool -ne 262144) {
            throw 'The audio smoke output is outside the expected limits.'
        }
    }
}

$orderedKeys = @($requiredPatterns.Keys)
for ($index = 1; $index -lt $orderedKeys.Count; $index++) {
    if ($positions[$orderedKeys[$index]] -le $positions[$orderedKeys[$index - 1]]) {
        throw 'The audio smoke lifecycle records are out of order.'
    }
}
Write-Host "Phase 3 live audio smoke log passed: samples=$samples clock_us=$clock completions=$completed pool_bytes=$pool"
