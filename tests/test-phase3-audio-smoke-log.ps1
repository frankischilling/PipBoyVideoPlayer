[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase3-audio-log-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $good = @'
10:00:00.000 [INFO] Private FFmpeg runtime accepted: avcodec=0x3E1C66
10:00:00.001 [INFO] PBVP_AUDIO_SMOKE_TEST_ARMED: playing the generated Phase 3 tone at volume 0.10
10:00:00.210 [INFO] Audio smoke playback started: prebuffer_ms=200 pool_bytes=262144
10:00:02.030 [INFO] Audio smoke passed: source_rate=44100 source_channels=2 output_rate=48000 output_channels=2 samples=96967 clock_us=2020125 underruns=0 completions=95 stream_ends=1 pool_bytes=262144 generation=1
10:00:02.031 [INFO] Audio smoke decoder worker joined before private FFmpeg unload
10:00:02.032 [INFO] Audio smoke voices and callback targets released
10:00:05.000 [INFO] Process shutdown requested
'@
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $badCases = [ordered]@{
        Error = $good.Replace(
            '10:00:02.030 [INFO] Audio smoke passed:',
            '10:00:02.029 [ERROR] audio failed' + [Environment]::NewLine +
            '10:00:02.030 [INFO] Audio smoke passed:')
        Path = $good.Replace('generated Phase 3', 'C:\Private\clip.mp4 generated Phase 3')
        Clock = $good.Replace('clock_us=2020125', 'clock_us=100')
        Underrun = $good.Replace('underruns=0', 'underruns=1')
        Pool = $good.Replace('pool_bytes=262144', 'pool_bytes=999')
        Order = $good.Replace(
            '10:00:02.032 [INFO] Audio smoke voices and callback targets released',
            '__PBVP_RELEASED__').Replace(
            '10:00:05.000 [INFO] Process shutdown requested',
            '10:00:02.032 [INFO] Audio smoke voices and callback targets released').Replace(
            '__PBVP_RELEASED__',
            '10:00:05.000 [INFO] Process shutdown requested')
    }
    foreach ($case in $badCases.GetEnumerator()) {
        $path = Join-Path $temporaryRoot ($case.Key + '.log')
        [IO.File]::WriteAllText($path, $case.Value, [Text.UTF8Encoding]::new($false))
        $rejected = $false
        try {
            & $CheckerPath -LogPath $path
        } catch {
            $rejected = $true
        }
        if (-not $rejected) {
            throw "The audio smoke checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 3 audio smoke log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
