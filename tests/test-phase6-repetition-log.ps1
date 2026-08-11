[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "pbvp-repetition-log-$([Guid]::NewGuid().ToString('N'))")
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null

try {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('10:00:00.000 [INFO] Pip-Boy Video Player 0.1.0 loading; runtime=0x01040020 xNVSE=0x06040050')
    $lines.Add('10:00:00.100 [INFO] Private FFmpeg runtime accepted: avcodec=0x3E1C66')
    $lines.Add('10:00:01.000 [INFO] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0')
    $lines.Add('10:00:01.100 [INFO] Decoded BGRA video reached PBVP_VideoSurface')
    for ($playback = 1; $playback -le 100; $playback++) {
        $lines.Add(
            "10:01:00.000 [INFO] Playback opened catalog item: session=123 bytes=49267 media=Fixture.mp4")
        $seeks = if ($playback -eq 100) { 40 } else { 0 }
        $forward = if ($playback -eq 100) { 20 } else { 0 }
        $backward = if ($playback -eq 100) { 20 } else { 0 }
        $generation = $seeks + 1
        $lines.Add(
            "10:01:01.000 [INFO] Playback session summary: playback=$playback terminal=stopped state=idle error=none failure_site=none generation=$generation decoded=20 presented=18 dropped=2 stale_video=0 audio_samples=9600 stale_audio=0 clock_us=200000 video_end_us=200000 underruns=0 seeks=$seeks forward_seeks=$forward backward_seeks=$backward pauses=0 resumes=0 buffering=1 max_update_gap_ms=20 staged_peak=589824 decoder_video_peak=1769472 decoder_audio_peak=32768")
    }
    $lines.Add('10:10:00.000 [INFO] Playback audio stopped and decoder worker joined')
    $lines.Add('10:10:00.001 [INFO] Renderer summary: callbacks=10000 visible=9000 devices=1 video-submitted=1800 video-uploaded=1800 mailbox-replaced=0 mailbox-cleared=0 upload-successes=1800 upload-attempts=1800 upload-failures=0 upload-us=18.00/25.00/60.00')
    $lines.Add('10:10:00.002 [INFO] Process shutdown requested')
    $good = $lines -join [Environment]::NewLine
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $lastSummary = $lines | Where-Object {
        $_ -match 'Playback session summary: playback=100 '
    } | Select-Object -First 1
    $joinedLine = '10:10:00.000 [INFO] Playback audio stopped and decoder worker joined'
    $rendererLine = '10:10:00.001 [INFO] Renderer summary: callbacks=10000 visible=9000 devices=1 video-submitted=1800 video-uploaded=1800 mailbox-replaced=0 mailbox-cleared=0 upload-successes=1800 upload-attempts=1800 upload-failures=0 upload-us=18.00/25.00/60.00'
    $badCases = [ordered]@{
        Warning = $good.Replace(
            '10:10:00.000 [INFO]',
            '10:09:59.999 [WARN] unexpected warning' + [Environment]::NewLine +
            '10:10:00.000 [INFO]')
        Path = $good.Replace('media=Fixture.mp4', 'media=C:\Private\Fixture.mp4')
        MissingSession = $good.Replace($lastSummary + [Environment]::NewLine, '')
        Sequence = $good.Replace('playback=50 terminal=', 'playback=49 terminal=')
        NoFrame = $good.Replace('decoded=20 presented=18', 'decoded=20 presented=0')
        Underrun = $good.Replace('underruns=0 seeks=40', 'underruns=1 seeks=40')
        Forward = $good.Replace('forward_seeks=20', 'forward_seeks=19')
        Backward = $good.Replace('backward_seeks=20', 'backward_seeks=19')
        Memory = $good.Replace('staged_peak=589824', 'staged_peak=8388609')
        Upload = $good.Replace('upload-failures=0', 'upload-failures=1')
        Order = $good.Replace(
            $joinedLine + [Environment]::NewLine + $rendererLine,
            $rendererLine + [Environment]::NewLine + $joinedLine)
    }
    foreach ($case in $badCases.GetEnumerator()) {
        $path = Join-Path $temporaryRoot "$($case.Key).log"
        [IO.File]::WriteAllText(
            $path,
            $case.Value,
            [Text.UTF8Encoding]::new($false))
        $rejected = $false
        try {
            & $CheckerPath -LogPath $path
        } catch {
            $rejected = $true
        }
        if (-not $rejected) {
            throw "The repetition checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 6 repetition log checker tests passed.'
} finally {
    if ((Test-Path -LiteralPath $temporaryRoot) -and
        [IO.Path]::GetFileName($temporaryRoot) -like 'pbvp-repetition-log-*' -and
        (Split-Path -Parent $temporaryRoot) -eq [IO.Path]::GetTempPath().TrimEnd('\')) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
