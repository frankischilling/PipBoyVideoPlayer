[CmdletBinding()]
param([Parameter(Mandatory)][string]$CheckerPath)

$ErrorActionPreference = 'Stop'
$checker = (Resolve-Path -LiteralPath $CheckerPath).Path
$shell = (Get-Process -Id $PID).Path
$tempRoot = [IO.Path]::GetFullPath(
    [IO.Path]::Combine([IO.Path]::GetTempPath(), "pbvp-phase1-log-$([Guid]::NewGuid().ToString('N'))"))
[IO.Directory]::CreateDirectory($tempRoot) | Out-Null

function Write-Fixture {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Text)
    $path = Join-Path $tempRoot $Name
    [IO.File]::WriteAllText($path, $Text.Replace("`n", "`r`n"))
    return $path
}

function Invoke-Checker {
    param(
        [Parameter(Mandatory)][string]$Fixture,
        [string[]]$Arguments = @()
    )
    & $shell -NoProfile -ExecutionPolicy Bypass -File $checker -LogPath $Fixture @Arguments 2>&1 |
        Out-Null
    return $LASTEXITCODE
}

try {
    $base = @'
12:00:00.000 [INFO] Pip-Boy Video Player 0.1.0 loading; runtime=0x040020D0 xNVSE=0x06040050
12:00:00.010 [INFO] Verified NiDX9Renderer::Recreate hook installed
12:00:01.000 [INFO] UIO video rectangle resolved: left=42.00 top=375.00 right=426.00 bottom=591.00 canvas=1706.67x960.00
12:00:01.010 [INFO] D3D device validated: adapter=Fixture driver=fixture.dll mode=fullscreen backbuffer=1920x1080 format=22 interval=0x00000001
12:00:01.020 [INFO] Engine texture checkerboard upload took 24.50 microseconds
12:00:01.021 [INFO] Generated checkerboard uploaded to PBVP_VideoSurface
12:00:02.000 [INFO] Process shutdown requested
'@
    $normal = Write-Fixture -Name 'normal.txt' -Text $base
    if ((Invoke-Checker -Fixture $normal -Arguments @(
            '-ExpectedWidth', '1920', '-ExpectedHeight', '1080', '-RequireCleanExit')) -ne 0) {
        throw 'The valid normal fixture failed.'
    }

    $resetText = $base.Replace(
        '12:00:02.000 [INFO] Process shutdown requested',
        "12:00:01.500 [INFO] Transient engine-surface state cleared before engine recreation 1`n" +
        "12:00:01.600 [INFO] D3D engine recreation applied the requested presentation parameters; resources will be reacquired`n" +
        '12:00:02.000 [INFO] Process shutdown requested')
    $reset = Write-Fixture -Name 'reset.txt' -Text $resetText
    if ((Invoke-Checker -Fixture $reset -Arguments @('-MinimumRecreates', '1')) -ne 0) {
        throw 'The valid recreation fixture failed.'
    }

    $errorFixture = Write-Fixture -Name 'error.txt' -Text (
        $base + "12:00:01.500 [ERROR] Fixture error`n")
    if ((Invoke-Checker -Fixture $errorFixture) -eq 0) {
        throw 'The error fixture was accepted.'
    }
    if ((Invoke-Checker -Fixture $normal -Arguments @(
            '-ExpectedWidth', '1280', '-ExpectedHeight', '720')) -eq 0) {
        throw 'The wrong backbuffer fixture was accepted.'
    }
    if ((Invoke-Checker -Fixture $normal -Arguments @('-MinimumRecreates', '1')) -eq 0) {
        throw 'The missing recreation fixture was accepted.'
    }

    Write-Host 'Phase 1 log checker tests passed.'
} finally {
    $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
    $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedTemp)) {
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    }
}
