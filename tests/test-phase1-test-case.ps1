[CmdletBinding()]
param([Parameter(Mandatory)][string]$TestCaseScript)

$ErrorActionPreference = 'Stop'
$script = (Resolve-Path -LiteralPath $TestCaseScript).Path
$tempRoot = [IO.Path]::GetFullPath(
    [IO.Path]::Combine([IO.Path]::GetTempPath(), "pbvp-case-test-$([Guid]::NewGuid().ToString('N'))"))
$profile = Join-Path $tempRoot 'profiles\PBVP Phase 1 Extended'
$rtssProfile = Join-Path $tempRoot 'rtss\FalloutNV.exe.cfg'
[IO.Directory]::CreateDirectory($profile) | Out-Null
[IO.Directory]::CreateDirectory((Split-Path -Parent $rtssProfile)) | Out-Null

$commonDisplay = "[Display]`r`nbFull Screen=1`r`niPresentInterval=1`r`n"
$prefsOriginal = $commonDisplay + "iSize W=1920`r`niSize H=1080`r`n"
$rtssOriginal = "[Framerate]`r`nLimit=0`r`nLimitDenominator=1`r`n"

try {
    [IO.File]::WriteAllText((Join-Path $profile 'fallout.ini'), $commonDisplay)
    [IO.File]::WriteAllText((Join-Path $profile 'falloutcustom.ini'), $commonDisplay)
    [IO.File]::WriteAllText((Join-Path $profile 'falloutprefs.ini'), $prefsOriginal)
    [IO.File]::WriteAllText(
        (Join-Path $profile 'modlist.txt'),
        "+Pip-Boy Video Player - Phase 1 Save Guard`r`n" +
        "+Pip-Boy Video Player - Dev`r`n")
    [IO.File]::WriteAllText($rtssProfile, $rtssOriginal)

    $guardRefused = $false
    try {
        [IO.File]::WriteAllText(
            (Join-Path $profile 'modlist.txt'),
            "+Pip-Boy Video Player - Dev`r`n")
        & $script -InstanceRoot $tempRoot `
            -ProfileName 'PBVP Phase 1 Extended' `
            -Width 1280 -Height 720 `
            -DisplayMode Windowed -VSync Off `
            -SkipFrameCap -Confirm:$false
    } catch {
        if ($_.Exception.Message -notmatch 'save guard is not enabled exactly once') {
            throw
        }
        $guardRefused = $true
    } finally {
        [IO.File]::WriteAllText(
            (Join-Path $profile 'modlist.txt'),
            "+Pip-Boy Video Player - Phase 1 Save Guard`r`n" +
            "+Pip-Boy Video Player - Dev`r`n")
    }
    if (-not $guardRefused) {
        throw 'The matrix configurator accepted a profile without the save guard.'
    }

    $permissionRefused = $false
    try {
        [IO.File]::SetAttributes($rtssProfile, [IO.FileAttributes]::ReadOnly)
        & $script -InstanceRoot $tempRoot `
            -ProfileName 'PBVP Phase 1 Extended' `
            -RtssProfilePath $rtssProfile `
            -Width 1280 -Height 720 `
            -DisplayMode Windowed -FpsCap 30 -VSync Off `
            -Confirm:$false
    } catch {
        if ($_.Exception.Message -notmatch 'test-case file is not writable') {
            throw
        }
        $permissionRefused = $true
    } finally {
        [IO.File]::SetAttributes($rtssProfile, [IO.FileAttributes]::Normal)
    }
    if (-not $permissionRefused) {
        throw 'The matrix configurator accepted a protected RTSS profile.'
    }
    if ([IO.File]::ReadAllText((Join-Path $profile 'falloutprefs.ini')) -cne $prefsOriginal -or
        [IO.File]::ReadAllText($rtssProfile) -cne $rtssOriginal -or
        @(Get-ChildItem -LiteralPath $tempRoot -Recurse -File | Where-Object {
            $_.Name -match '\.pbvp-phase1\.(?:bak|state\.json)$'
        }).Count -ne 0) {
        throw 'The protected-file refusal changed a test file or left temporary state.'
    }

    & $script -InstanceRoot $tempRoot `
        -ProfileName 'PBVP Phase 1 Extended' `
        -RtssProfilePath $rtssProfile `
        -Width 1280 -Height 720 `
        -DisplayMode Windowed -FpsCap 30 -VSync Off `
        -Confirm:$false

    $prefs = Get-Content -LiteralPath (Join-Path $profile 'falloutprefs.ini') -Raw
    $custom = Get-Content -LiteralPath (Join-Path $profile 'falloutcustom.ini') -Raw
    $rtss = Get-Content -LiteralPath $rtssProfile -Raw
    if ($prefs -notmatch '(?m)^iSize W=1280\r?$' -or
        $prefs -notmatch '(?m)^iSize H=720\r?$' -or
        $prefs -notmatch '(?m)^bFull Screen=0\r?$' -or
        $custom -notmatch '(?m)^bFull Screen=0\r?$' -or
        $custom -notmatch '(?m)^iPresentInterval=0\r?$' -or
        $rtss -notmatch '(?m)^Limit=30\r?$') {
        throw 'The first matrix case was not applied correctly.'
    }

    & $script -InstanceRoot $tempRoot `
        -ProfileName 'PBVP Phase 1 Extended' `
        -RtssProfilePath $rtssProfile `
        -Width 3440 -Height 1440 `
        -DisplayMode Fullscreen -FpsCap 120 -VSync On `
        -Confirm:$false

    $prefs = Get-Content -LiteralPath (Join-Path $profile 'falloutprefs.ini') -Raw
    $rtss = Get-Content -LiteralPath $rtssProfile -Raw
    if ($prefs -notmatch '(?m)^iSize W=3440\r?$' -or
        $prefs -notmatch '(?m)^iSize H=1440\r?$' -or
        $prefs -notmatch '(?m)^bFull Screen=1\r?$' -or
        $prefs -notmatch '(?m)^iPresentInterval=1\r?$' -or
        $rtss -notmatch '(?m)^Limit=120\r?$') {
        throw 'The second matrix case was not applied correctly.'
    }

    $refused = $false
    try {
        & $script -InstanceRoot $tempRoot `
            -ProfileName 'PBVP Phase 1 Extended' `
            -RtssProfilePath $rtssProfile `
            -Width 1920 -Height 1200 `
            -DisplayMode Fullscreen -FpsCap 60 -VSync On `
            -Confirm:$false
    } catch {
        if ($_.Exception.Message -notmatch 'Unsupported Phase 1 test resolution') {
            throw
        }
        $refused = $true
    }
    if (-not $refused) {
        throw 'The matrix configurator accepted an undocumented resolution.'
    }

    & $script -InstanceRoot $tempRoot `
        -ProfileName 'PBVP Phase 1 Extended' `
        -RtssProfilePath $rtssProfile `
        -Restore -Confirm:$false

    if ([IO.File]::ReadAllText((Join-Path $profile 'falloutprefs.ini')) -cne $prefsOriginal -or
        [IO.File]::ReadAllText((Join-Path $profile 'falloutcustom.ini')) -cne $commonDisplay -or
        [IO.File]::ReadAllText($rtssProfile) -cne $rtssOriginal) {
        throw 'The matrix configurator did not restore the original files byte for byte.'
    }
    if (@(Get-ChildItem -LiteralPath $tempRoot -Recurse -File | Where-Object {
            $_.Name -match '\.pbvp-phase1\.(?:bak|state\.json)$'
        }).Count -ne 0) {
        throw 'The matrix configurator left backup or state files after restoration.'
    }

    & $script -InstanceRoot $tempRoot `
        -ProfileName 'PBVP Phase 1 Extended' `
        -Width 1280 -Height 960 `
        -DisplayMode Windowed -VSync Off `
        -SkipFrameCap -Confirm:$false

    $prefs = Get-Content -LiteralPath (Join-Path $profile 'falloutprefs.ini') -Raw
    if ($prefs -notmatch '(?m)^iSize W=1280\r?$' -or
        $prefs -notmatch '(?m)^iSize H=960\r?$' -or
        $prefs -notmatch '(?m)^bFull Screen=0\r?$' -or
        $prefs -notmatch '(?m)^iPresentInterval=0\r?$' -or
        [IO.File]::ReadAllText($rtssProfile) -cne $rtssOriginal) {
        throw 'The display-only matrix case changed the wrong settings.'
    }

    & $script -InstanceRoot $tempRoot `
        -ProfileName 'PBVP Phase 1 Extended' `
        -SkipFrameCap -Restore -Confirm:$false

    if ([IO.File]::ReadAllText((Join-Path $profile 'falloutprefs.ini')) -cne $prefsOriginal -or
        [IO.File]::ReadAllText((Join-Path $profile 'falloutcustom.ini')) -cne $commonDisplay -or
        [IO.File]::ReadAllText($rtssProfile) -cne $rtssOriginal -or
        @(Get-ChildItem -LiteralPath $tempRoot -Recurse -File | Where-Object {
            $_.Name -match '\.pbvp-phase1(?:-display)?\.(?:bak|state\.json)$'
        }).Count -ne 0) {
        throw 'The display-only matrix case did not restore its files.'
    }

    Write-Host 'Phase 1 test-case configurator tests passed.'
} finally {
    $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
    $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedTemp)) {
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    }
}
