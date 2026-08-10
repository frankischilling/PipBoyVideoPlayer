[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase2-log-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $good = @'
10:00:00.000 [INFO] Pip-Boy Video Player 0.1.0 loading
10:00:00.001 [INFO] Private FFmpeg runtime accepted: avcodec=0x3E1F64 avformat=0x3E0B64 avutil=0x3C1264 swresample=0x060164 swscale=0x090164
10:00:00.002 [INFO] PBVP_MEDIA_SMOKE_TEST_ARMED: waiting for a stable main-menu memory baseline
10:00:05.002 [INFO] Media smoke no-decode memory control started after five-second settle delay
10:00:06.002 [INFO] Media smoke no-decode control passed: private_delta=1048576
10:00:06.003 [INFO] Media smoke decoder worker started for PBVP-Phase2-Smoke.mp4
10:00:06.050 [INFO] Media smoke passed: source=1920x1080 video=30 audio_chunks=48 audio_samples=48128 private_delta=68000000 video_queue_peak=24883200 audio_queue_peak=28672 generation=1
10:00:06.051 [INFO] Media smoke worker joined before private FFmpeg unload
10:00:10.000 [INFO] Process shutdown requested
'@
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $badCases = [ordered]@{
        Error = $good.Replace(
            '10:00:06.050 [INFO] Media smoke passed:',
            '10:00:06.049 [ERROR] decoder failed' + [Environment]::NewLine +
            '10:00:06.050 [INFO] Media smoke passed:')
        Path = $good.Replace('stable main-menu', 'C:\Private\video.mp4 stable main-menu')
        Control = $good.Replace(
            'no-decode control passed: private_delta=1048576',
            'no-decode control passed: private_delta=50000000')
        Count = $good.Replace('audio_samples=48128', 'audio_samples=100')
        Order = $good.Replace(
            '10:00:06.051 [INFO] Media smoke worker joined before private FFmpeg unload',
            '10:00:20.000 [INFO] Process shutdown requested').Replace(
            '10:00:10.000 [INFO] Process shutdown requested',
            '10:00:20.001 [INFO] Media smoke worker joined before private FFmpeg unload')
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
            throw "The checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 2 media smoke log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
