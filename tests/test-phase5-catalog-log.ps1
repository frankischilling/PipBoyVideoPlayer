[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase5-log-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $good = @'
10:00:00.000 [INFO] Private FFmpeg runtime accepted: avcodec=0x3E1C66
10:00:00.001 [INFO] Configuration accepted: enabled=1 aspect=fit tint=pipboy volume=1.00 muted=0 seek_seconds=10 catalog=500 display_chars=128 source=1920x1080 queued_edge=512 file_limit=34359738368 logging=normal
10:00:01.000 [INFO] Scoped MapMenu input bridge attached after vtable validation
10:00:02.000 [INFO] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0
10:00:03.000 [INFO] Playback opened catalog item: session=123 bytes=49267
10:00:03.100 [INFO] Decoded BGRA video reached PBVP_VideoSurface
10:00:04.000 [INFO] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0
10:00:05.000 [INFO] Playback opened catalog item: session=456 bytes=49267
10:00:06.000 [INFO] Playback audio stopped and decoder worker joined
10:00:06.001 [INFO] Renderer summary: callbacks=500 visible=400 devices=1
10:00:06.002 [INFO] Process shutdown requested
'@
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $badCases = [ordered]@{
        Error = $good.Replace(
            '10:00:03.100 [INFO] Decoded',
            '10:00:03.099 [ERROR] failed' + [Environment]::NewLine +
            '10:00:03.100 [INFO] Decoded')
        Warning = $good.Replace(
            '10:00:03.100 [INFO] Decoded',
            '10:00:03.099 [WARN] unsafe' + [Environment]::NewLine +
            '10:00:03.100 [INFO] Decoded')
        Path = $good.Replace('session=123', 'C:\Private\clip.mp4 session=123')
        Metadata = $good.Replace('session=123', 'title=Private session=123')
        OneScan = $good.Replace(
            '10:00:04.000 [INFO] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0', '')
        OneStart = $good.Replace(
            '10:00:05.000 [INFO] Playback opened catalog item: session=456 bytes=49267', '')
        Entries = $good.Replace('entries=10', 'entries=9')
        Order = [regex]::Replace(
            $good,
            '10:00:04\.000 \[INFO\] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0\r?\n' +
            '10:00:05\.000 \[INFO\] Playback opened catalog item: session=456 bytes=49267',
            '10:00:04.000 [INFO] Playback opened catalog item: session=456 bytes=49267' + [Environment]::NewLine +
            '10:00:05.000 [INFO] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0')
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
            throw "The Phase 5 log checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 5 catalog log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
