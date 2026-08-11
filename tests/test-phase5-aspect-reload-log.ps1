[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase5-aspect-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $good = @'
10:00:00.000 [INFO] Configuration accepted: enabled=1 aspect=fit tint=pipboy volume=1.00 muted=0 seek_seconds=10 catalog=500 display_chars=128 source=1920x1080 queued_edge=512 file_limit=34359738368 logging=normal
10:00:01.000 [INFO] Playback opened catalog item: session=123 bytes=767230 media=Aspect Mode 4x3.mp4
10:00:01.010 [INFO] Playback stream summary: source=160x120 display=160x120 rotation=0 duration_us=30000000 audio=1 source_audio=2ch@48000 output_audio=2ch@48000
10:00:03.000 [INFO] Configuration accepted: enabled=1 aspect=fill tint=pipboy volume=1.00 muted=0 seek_seconds=10 catalog=500 display_chars=128 source=1920x1080 queued_edge=512 file_limit=34359738368 logging=normal
10:00:03.010 [INFO] Configuration reloaded while playback was idle
10:00:04.000 [INFO] Playback opened catalog item: session=123 bytes=767230 media=Aspect Mode 4x3.mp4
10:00:04.010 [INFO] Playback stream summary: source=160x120 display=160x120 rotation=0 duration_us=30000000 audio=1 source_audio=2ch@48000 output_audio=2ch@48000
10:00:06.000 [INFO] Process shutdown requested
'@
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $firstOpenLine = '10:00:01.000 [INFO] Playback opened catalog item: session=123 bytes=767230 media=Aspect Mode 4x3.mp4'
    $fillLine = '10:00:03.000 [INFO] Configuration accepted: enabled=1 aspect=fill tint=pipboy volume=1.00 muted=0 seek_seconds=10 catalog=500 display_chars=128 source=1920x1080 queued_edge=512 file_limit=34359738368 logging=normal'
    $wrongOrder = $good.Replace($firstOpenLine, '__PBVP_FIRST_OPEN__')
    $wrongOrder = $wrongOrder.Replace($fillLine, $firstOpenLine)
    $wrongOrder = $wrongOrder.Replace('__PBVP_FIRST_OPEN__', $fillLine)
    $badCases = [ordered]@{
        Warning = $good.Replace(
            '10:00:01.010 [INFO] Playback stream',
            '10:00:01.005 [WARN] unexpected' + [Environment]::NewLine +
            '10:00:01.010 [INFO] Playback stream')
        Path = $good.Replace(
            'media=Aspect Mode 4x3.mp4',
            'media=C:\Private\Aspect Mode 4x3.mp4')
        Metadata = $good.Replace(
            'session=123 bytes=767230',
            'title=Private session=123 bytes=767230')
        NoFill = $good.Replace($fillLine, '')
        OneOpen = [regex]::Replace(
            $good,
            '10:00:04\.000 \[INFO\] Playback opened catalog item:.*\r?\n',
            '')
        OneStream = [regex]::Replace(
            $good,
            '10:00:04\.010 \[INFO\] Playback stream summary:.*\r?\n',
            '')
        WrongOrder = $wrongOrder
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
            throw "The Phase 5 aspect and reload checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 5 aspect and reload checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
