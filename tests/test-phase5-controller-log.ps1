[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase5-controller-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $good = @'
10:00:00.000 [INFO] Scoped MapMenu input bridge attached after vtable validation; table=verified-stewie-menu-search-chain
10:00:01.000 [INFO] Video catalog scan finished: status=ok entries=10 truncated=0 win32=0
10:00:02.000 [INFO] Videos input method changed: controller
10:00:03.000 [INFO] Playback opened catalog item: session=123 bytes=49267 media=Episode 1.mp4
10:00:03.010 [INFO] Playback stream summary: source=320x180 display=320x180 rotation=0 duration_us=30000000 audio=1 source_audio=2ch@48000 output_audio=2ch@48000
10:00:04.000 [INFO] Videos input method changed: keyboard-mouse
10:00:05.000 [INFO] Process shutdown requested
'@
    $goodPath = Join-Path $temporaryRoot 'good.log'
    [IO.File]::WriteAllText($goodPath, $good, [Text.UTF8Encoding]::new($false))
    & $CheckerPath -LogPath $goodPath

    $controllerLine = '10:00:02.000 [INFO] Videos input method changed: controller'
    $openLine = '10:00:03.000 [INFO] Playback opened catalog item: session=123 bytes=49267 media=Episode 1.mp4'
    $wrongOrder = $good.Replace($controllerLine, '__PBVP_CONTROLLER__')
    $wrongOrder = $wrongOrder.Replace($openLine, $controllerLine)
    $wrongOrder = $wrongOrder.Replace('__PBVP_CONTROLLER__', $openLine)
    $badCases = [ordered]@{
        Warning = $good.Replace(
            '10:00:03.010 [INFO] Playback stream',
            '10:00:03.005 [WARN] unexpected' + [Environment]::NewLine +
            '10:00:03.010 [INFO] Playback stream')
        Path = $good.Replace(
            'media=Episode 1.mp4',
            'media=C:\Private\Episode 1.mp4')
        Metadata = $good.Replace(
            'session=123 bytes=49267',
            'artist=Private session=123 bytes=49267')
        EmptyCatalog = $good.Replace('entries=10', 'entries=0')
        OversizedCatalog = $good.Replace('entries=10', 'entries=501')
        NoController = [regex]::Replace(
            $good,
            '10:00:02\.000 \[INFO\] Videos input method changed: controller\r?\n',
            '')
        NoKeyboardMouse = [regex]::Replace(
            $good,
            '10:00:04\.000 \[INFO\] Videos input method changed: keyboard-mouse\r?\n',
            '')
        NoOpen = [regex]::Replace(
            $good,
            '10:00:03\.000 \[INFO\] Playback opened catalog item:.*\r?\n',
            '')
        NoStream = [regex]::Replace(
            $good,
            '10:00:03\.010 \[INFO\] Playback stream summary:.*\r?\n',
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
            throw "The Phase 5 controller checker accepted the $($case.Key) failure fixture."
        }
    }
    Write-Host 'Phase 5 controller log checker tests passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
