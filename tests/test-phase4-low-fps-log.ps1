[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase4-low-fps-log-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $goodLines = @(
        '10:00:00.000 [INFO] Private FFmpeg runtime accepted: avcodec=0x3E1C66',
        '10:00:01.000 [INFO] PBVP_PLAYBACK_LONG_TEST_ARMED: opening the generated 30-minute 720p30 fixture at volume 0.03',
        '10:00:01.100 [INFO] Decoded BGRA video reached PBVP_VideoSurface',
        '10:05:01.000 [INFO] Integrated playback long test progress: clock_us=299930000 decoded=9000 presented=2995 dropped=6004 underruns=0 private_delta=42000000',
        '10:05:03.000 [INFO] Playback audio stopped and decoder worker joined',
        '10:05:03.001 [INFO] Renderer summary: callbacks=3050 visible=3000 devices=1 video-submitted=2995 video-uploaded=2994 mailbox-replaced=0 mailbox-cleared=1 upload-successes=2995 upload-attempts=2995 upload-failures=0 upload-us=18.00/25.00/150.00',
        '10:05:03.002 [INFO] Visible cadence summary: samples=8 fps=9.80/10.00/10.20',
        '10:05:03.003 [INFO] Process shutdown requested'
    )
    $good = $goodLines -join [Environment]::NewLine
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $noClear = $good.Replace(
        'video-uploaded=2994 mailbox-replaced=0 mailbox-cleared=1',
        'video-uploaded=2995 mailbox-replaced=0 mailbox-cleared=0')
    $noClearPath = Join-Path $temporaryRoot 'good-no-clear.log'
    [IO.File]::WriteAllText($noClearPath, $noClear, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $noClearPath

    $badCases = [ordered]@{
        Error = $good.Replace(
            '10:05:03.000 [INFO] Playback audio stopped',
            '10:05:02.999 [ERROR] playback failed' + [Environment]::NewLine +
            '10:05:03.000 [INFO] Playback audio stopped')
        Path = $good.Replace('generated 30-minute', 'C:\Private\clip.mp4 generated 30-minute')
        Clock = $good.Replace('clock_us=299930000', 'clock_us=298999999')
        Drops = $good.Replace('dropped=6004', 'dropped=4999')
        Underrun = $good.Replace('underruns=0', 'underruns=1')
        Memory = $good.Replace('private_delta=42000000', 'private_delta=134217728')
        Upload = $good.Replace('video-uploaded=2994', 'video-uploaded=0')
        Replacement = $good.Replace('mailbox-replaced=0', 'mailbox-replaced=1')
        Cleared = $good.Replace('mailbox-cleared=1', 'mailbox-cleared=2')
        Unaccounted = $good.Replace('video-uploaded=2994', 'video-uploaded=2993')
        Cadence = $good.Replace('fps=9.80/10.00/10.20', 'fps=59.80/60.00/60.20')
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
            throw "The low-FPS checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 4 low-FPS log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
