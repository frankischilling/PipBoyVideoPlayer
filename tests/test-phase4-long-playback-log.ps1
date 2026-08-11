[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase4-long-log-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $progress = 1..5 | ForEach-Object {
        $clock = $_ * 300000000
        "10:0$($_):00.000 [INFO] Integrated playback long test progress: clock_us=$clock decoded=$($_ * 9000) presented=$($_ * 8990) dropped=$($_ * 10) underruns=0 private_delta=70000000"
    }
    $goodLines = [Collections.Generic.List[string]]::new()
    $goodLines.Add('10:00:00.000 [INFO] Private FFmpeg runtime accepted: avcodec=0x3E1C66')
    $goodLines.Add('10:00:01.000 [INFO] PBVP_PLAYBACK_LONG_TEST_ARMED: opening the generated 30-minute 720p30 fixture at volume 0.03')
    $goodLines.Add('10:00:01.100 [INFO] Decoded BGRA video reached PBVP_VideoSurface')
    $goodLines.AddRange([string[]]$progress)
    $goodLines.Add('10:30:02.100 [INFO] Integrated playback long test passed: decoded=54000 presented=53950 dropped=50 audio_samples=86401024 clock_us=1799971875 video_end_us=1800000000 sync_error_us=28125 underruns=0 seeks=0 video_submitted=53950 video_uploaded=53950 mailbox_replaced=0 upload_us=20.00/25.00/40.00 private_delta=70000000 staged_peak=3686400 decoder_video_peak=11059200 decoder_audio_peak=32768 generation=1')
    $goodLines.Add('10:30:03.000 [INFO] Playback audio stopped and decoder worker joined')
    $goodLines.Add('10:30:03.001 [INFO] Renderer summary: callbacks=250000 visible=200000 devices=1')
    $goodLines.Add('10:30:03.002 [INFO] Process shutdown requested')
    $good = $goodLines -join [Environment]::NewLine
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $badCases = [ordered]@{
        Error = $good.Replace(
            '10:30:02.100 [INFO] Integrated playback long test passed:',
            '10:30:02.099 [ERROR] playback failed' + [Environment]::NewLine +
            '10:30:02.100 [INFO] Integrated playback long test passed:')
        Path = $good.Replace('generated 30-minute', 'C:\Private\clip.mp4 generated 30-minute')
        ClockEarly = $good.Replace('clock_us=1799971875', 'clock_us=1799949999')
        ClockLate = $good.Replace('clock_us=1799971875', 'clock_us=1800050001')
        Drift = $good.Replace('sync_error_us=28125', 'sync_error_us=50001')
        Memory = $good.Replace('private_delta=70000000 staged_peak', 'private_delta=134217728 staged_peak')
        Underrun = $good.Replace('underruns=0 seeks=0', 'underruns=1 seeks=0')
        Progress = $good.Replace($progress[4] + [Environment]::NewLine, '')
        Upload = $good.Replace('video_uploaded=53950', 'video_uploaded=0')
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
            throw "The long playback checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 4 long playback log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
