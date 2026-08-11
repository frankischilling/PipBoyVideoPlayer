[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase5-ui-logs-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $good = @'
10:00:00.000 [INFO] Configuration accepted: enabled=1 aspect=fit tint=pipboy volume=1.00 muted=0 seek_seconds=10 catalog=500 display_chars=128 source=1920x1080 queued_edge=512 file_limit=34359738368 logging=normal
10:00:00.010 [INFO] Private FFmpeg runtime accepted: avutil=60 avcodec=62 avformat=62 swscale=9 swresample=6
10:00:01.000 [INFO] Scoped MapMenu input bridge attached after vtable validation; table=verified-stewie-menu-search-chain
10:00:02.000 [INFO] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0
10:00:03.000 [INFO] Playback opened catalog item: session=123 bytes=49267 media=Episode 1.mp4
10:00:03.010 [INFO] Playback stream summary: source=320x180 display=320x180 rotation=0 duration_us=30000000 audio=1 source_audio=2ch@48000 output_audio=2ch@48000
10:00:03.020 [INFO] Decoded BGRA video reached PBVP_VideoSurface
10:00:05.000 [INFO] Playback audio stopped and decoder worker joined
10:00:05.010 [INFO] Renderer summary: callbacks=100 visible=90 devices=1 video-submitted=20 video-uploaded=20 mailbox-replaced=0 mailbox-cleared=0 upload-successes=21 upload-attempts=21 upload-failures=0 upload-us=20.00/25.00/30.00
10:00:05.020 [INFO] Process shutdown requested
'@
    $paths = [ordered]@{}
    foreach ($name in @('base', 'vui-plus', 'clean-vanilla-hud', 'extended')) {
        $path = Join-Path $temporaryRoot "$name.log"
        [IO.File]::WriteAllText($path, $good, [Text.UTF8Encoding]::new($false))
        $paths[$name] = $path
    }

    function Invoke-Checker {
        param([Parameter(Mandatory)]$SelectedPaths)
        & $CheckerPath `
            -BaseLog $SelectedPaths['base'] `
            -VuiPlusLog $SelectedPaths['vui-plus'] `
            -CleanVanillaHudLog $SelectedPaths['clean-vanilla-hud'] `
            -ExtendedLog $SelectedPaths['extended']
    }

    Invoke-Checker -SelectedPaths $paths

    $startupPattern = '(?m)^(10:00:00\.000 \[INFO\] Configuration accepted:.*)\r?\n(10:00:00\.010 \[INFO\] Private FFmpeg runtime accepted:.*)$'
    $reversedStartup = $good -replace $startupPattern, (
        '$2' + [Environment]::NewLine + '$1')
    if ($reversedStartup -ceq $good) {
        throw 'The Phase 5 UI matrix startup-order failure fixture was not constructed.'
    }
    $badCases = [ordered]@{
        StartupOrder = $reversedStartup
        Warning = $good.Replace(
            '10:00:03.020 [INFO] Decoded',
            '10:00:03.015 [WARN] unexpected' + [Environment]::NewLine +
            '10:00:03.020 [INFO] Decoded')
        Path = $good.Replace(
            'media=Episode 1.mp4',
            'media=C:\Private\Episode 1.mp4')
        Metadata = $good.Replace(
            'session=123 bytes=49267',
            'comment=Private session=123 bytes=49267')
        EmptyCatalog = $good.Replace('entries=10', 'entries=0')
        MissingSurface = [regex]::Replace(
            $good,
            '10:00:03\.020 \[INFO\] Decoded BGRA video reached PBVP_VideoSurface\r?\n',
            '')
        UploadFailure = $good.Replace('upload-failures=0', 'upload-failures=1')
        NoUploads = $good.Replace(
            'upload-successes=21 upload-attempts=21',
            'upload-successes=0 upload-attempts=0')
        MissingShutdown = [regex]::Replace(
            $good,
            '10:00:05\.020 \[INFO\] Process shutdown requested\r?\n?',
            '')
    }
    foreach ($case in $badCases.GetEnumerator()) {
        $badPath = Join-Path $temporaryRoot ("bad-$($case.Key).log")
        [IO.File]::WriteAllText($badPath, $case.Value, [Text.UTF8Encoding]::new($false))
        $selected = [ordered]@{}
        foreach ($pathEntry in $paths.GetEnumerator()) {
            $selected[$pathEntry.Key] = $pathEntry.Value
        }
        $selected['extended'] = $badPath
        $rejected = $false
        try {
            Invoke-Checker -SelectedPaths $selected
        } catch {
            $rejected = $true
        }
        if (-not $rejected) {
            throw "The Phase 5 UI matrix checker accepted the $($case.Key) failure fixture."
        }
    }

    $duplicate = [ordered]@{}
    foreach ($pathEntry in $paths.GetEnumerator()) {
        $duplicate[$pathEntry.Key] = $pathEntry.Value
    }
    $duplicate['extended'] = $duplicate['base']
    $duplicateRejected = $false
    try {
        Invoke-Checker -SelectedPaths $duplicate
    } catch {
        $duplicateRejected = $true
    }
    if (-not $duplicateRejected) {
        throw 'The Phase 5 UI matrix checker accepted one log for two profiles.'
    }

    Write-Host 'Phase 5 UI matrix log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
