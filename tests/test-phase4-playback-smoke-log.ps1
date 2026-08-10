[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase4-log-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $good = @'
10:00:00.000 [INFO] Private FFmpeg runtime accepted: avcodec=0x3E1C66
10:00:01.000 [INFO] PBVP_PLAYBACK_SMOKE_TEST_ARMED: opening the generated Phase 4 fixture at volume 0.10
10:00:01.100 [INFO] Decoded BGRA video reached PBVP_VideoSurface
10:00:03.100 [INFO] Integrated playback smoke passed: decoded=20 presented=18 dropped=2 audio_samples=96967 clock_us=2020125 underruns=0 seeks=0 video_submitted=18 video_uploaded=18 mailbox_replaced=0 upload_us=20.00/25.00/30.00 generation=1
10:00:04.000 [INFO] Playback audio stopped and decoder worker joined
10:00:04.001 [INFO] Renderer summary: callbacks=500 visible=400 devices=1
10:00:04.002 [INFO] Process shutdown requested
'@
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $badCases = [ordered]@{
        Error = $good.Replace(
            '10:00:03.100 [INFO] Integrated playback smoke passed:',
            '10:00:03.099 [ERROR] playback failed' + [Environment]::NewLine +
            '10:00:03.100 [INFO] Integrated playback smoke passed:')
        Path = $good.Replace('generated Phase 4', 'C:\Private\clip.mp4 generated Phase 4')
        Samples = $good.Replace('audio_samples=96967', 'audio_samples=100')
        Clock = $good.Replace('clock_us=2020125', 'clock_us=100')
        Underrun = $good.Replace('underruns=0', 'underruns=1')
        Upload = $good.Replace('video_uploaded=18', 'video_uploaded=0')
        Cost = $good.Replace('upload_us=20.00/25.00/30.00', 'upload_us=20.00/25.00/10000.00')
        Order = $good.Replace(
            '10:00:04.000 [INFO] Playback audio stopped and decoder worker joined',
            '__PBVP_JOINED__').Replace(
            '10:00:04.001 [INFO] Renderer summary: callbacks=500 visible=400 devices=1',
            '10:00:04.000 [INFO] Playback audio stopped and decoder worker joined').Replace(
            '__PBVP_JOINED__',
            '10:00:04.001 [INFO] Renderer summary: callbacks=500 visible=400 devices=1')
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
            throw "The playback smoke checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 4 playback smoke log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
