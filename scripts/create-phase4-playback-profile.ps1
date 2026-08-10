[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$InstanceRoot,
    [string]$SourceProfile = 'PBVP Phase 1 Extended',
    [string]$TargetProfile = 'PBVP Phase 4 Playback',
    [string]$FixturePath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'tests\fixtures\h264-aac-44100-stereo.mp4'),
    [switch]$VerifyOnly,
    [switch]$SelectProfile
)

$ErrorActionPreference = 'Stop'
if ($VerifyOnly -and $SelectProfile) {
    throw 'VerifyOnly cannot select the MO2 profile.'
}

$instance = (Resolve-Path -LiteralPath $InstanceRoot).Path
$profiles = Join-Path $instance 'profiles'
$mods = Join-Path $instance 'mods'
$organizerIni = Join-Path $instance 'ModOrganizer.ini'
foreach ($required in @($profiles, $mods)) {
    if (-not (Test-Path -LiteralPath $required -PathType Container)) {
        throw "The selected MO2 instance is missing a required directory: $required"
    }
}

function Resolve-ChildPath {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Name
    )
    $candidate = [IO.Path]::GetFullPath((Join-Path $Root $Name))
    if (-not $candidate.StartsWith(
            $Root + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Resolved path escaped its expected root: $Name"
    }
    return $candidate
}

function Test-InstanceOrganizerRunning {
    foreach ($process in Get-Process -Name 'ModOrganizer' -ErrorAction SilentlyContinue) {
        try {
            if (-not [string]::IsNullOrWhiteSpace($process.Path) -and
                [IO.Path]::GetFullPath((Split-Path -Parent $process.Path)).TrimEnd(
                    [IO.Path]::DirectorySeparatorChar) -ieq $instance.TrimEnd(
                    [IO.Path]::DirectorySeparatorChar)) {
                return $true
            }
        } catch {
            continue
        }
    }
    return $false
}

if (-not $VerifyOnly -and (Test-InstanceOrganizerRunning)) {
    throw 'Close Mod Organizer before preparing the isolated Phase 4 playback profile.'
}

$fixture = (Resolve-Path -LiteralPath $FixturePath).Path
$expectedFixtureHash = '6549BA517226F8CB0CFB70F3B87489B58AA6ECCFCAB4DB75F808BD219DA1A068'
if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -cne $expectedFixtureHash) {
    throw 'The Phase 4 playback fixture hash does not match the canonical generated file.'
}

$playbackModName = 'Pip-Boy Video Player - Playback Test'
$playbackRelativePath = 'NVSE\Plugins\PipBoyVideoPlayer\Videos\PBVP-Phase4-Playback.mp4'
$playbackMod = Resolve-ChildPath -Root $mods -Name $playbackModName
$playbackTarget = Join-Path $playbackMod $playbackRelativePath
$source = Resolve-ChildPath -Root $profiles -Name $SourceProfile
$target = Resolve-ChildPath -Root $profiles -Name $TargetProfile

if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "The source test profile is missing: $SourceProfile"
}

if (-not $VerifyOnly) {
    if (-not (Test-Path -LiteralPath $playbackMod)) {
        if (-not $PSCmdlet.ShouldProcess($playbackMod, 'Create the isolated generated-playback mod')) {
            return
        }
        [IO.Directory]::CreateDirectory((Split-Path -Parent $playbackTarget)) | Out-Null
    } elseif (-not (Test-Path -LiteralPath $playbackMod -PathType Container)) {
        throw 'The Phase 4 playback mod path is not a directory.'
    }
    $unexpectedFiles = @(Get-ChildItem -LiteralPath $playbackMod -Recurse -File | Where-Object {
        $_.FullName -cne $playbackTarget
    })
    if ($unexpectedFiles.Count -ne 0) {
        throw 'The isolated Phase 4 playback mod contains an unexpected file.'
    }
    if ($PSCmdlet.ShouldProcess($playbackTarget, 'Install the canonical generated MP4 fixture')) {
        [IO.Directory]::CreateDirectory((Split-Path -Parent $playbackTarget)) | Out-Null
        Copy-Item -LiteralPath $fixture -Destination $playbackTarget -Force
    }

    if (-not (Test-Path -LiteralPath $target)) {
        if (-not $PSCmdlet.ShouldProcess($target, "Create $TargetProfile without saves")) {
            return
        }
        [IO.Directory]::CreateDirectory($target) | Out-Null
        Get-ChildItem -LiteralPath $source -File | Where-Object {
            $_.Name -notmatch '\.\d{4}_\d{2}_\d{2}_'
        } | Copy-Item -Destination $target
        [IO.Directory]::CreateDirectory((Join-Path $target 'saves')) | Out-Null
    } elseif (-not (Test-Path -LiteralPath $target -PathType Container)) {
        throw 'The Phase 4 profile path is not a directory.'
    }

    $saves = Join-Path $target 'saves'
    if ((Test-Path -LiteralPath $saves -PathType Container) -and
        @(Get-ChildItem -LiteralPath $saves -Force).Count -ne 0) {
        throw 'Refusing to update a Phase 4 profile that contains save data.'
    }

    $modList = Join-Path $target 'modlist.txt'
    if (-not (Test-Path -LiteralPath $modList -PathType Leaf)) {
        throw 'The Phase 4 profile has no modlist.txt.'
    }
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in Get-Content -LiteralPath $modList) {
        $lines.Add($line)
    }
    $comment = '# PBVP Phase 4 playback test profile. Existing VNV profiles remain unchanged.'
    $insertAt = if ($lines.Count -gt 0 -and
        $lines[0] -eq '# This file was automatically generated by Mod Organizer.') { 1 } else { 0 }
    if (-not $lines.Contains($comment)) {
        $lines.Insert($insertAt, $comment)
        $insertAt++
    }
    foreach ($modName in @(
            'Pip-Boy Video Player - Phase 1 Save Guard',
            'Pip-Boy Video Player - Dev',
            $playbackModName)) {
        for ($index = $lines.Count - 1; $index -ge 0; $index--) {
            if ($lines[$index] -eq "+$modName" -or $lines[$index] -eq "-$modName") {
                $lines.RemoveAt($index)
            }
        }
        $lines.Insert($insertAt, "+$modName")
        $insertAt++
    }
    [IO.File]::WriteAllLines($modList, $lines, [Text.UTF8Encoding]::new($false))

    if ($SelectProfile) {
        if (-not (Test-Path -LiteralPath $organizerIni -PathType Leaf)) {
            throw 'ModOrganizer.ini is missing, so the Phase 4 profile cannot be selected.'
        }
        $ini = [IO.File]::ReadAllText($organizerIni)
        if ($ini -notmatch '(?m)^selected_profile=.*$') {
            throw 'ModOrganizer.ini has no selected_profile setting.'
        }
        $ini = [Text.RegularExpressions.Regex]::Replace(
            $ini, '(?m)^selected_profile=.*$',
            "selected_profile=@ByteArray($TargetProfile)", 1)
        [IO.File]::WriteAllText($organizerIni, $ini, [Text.UTF8Encoding]::new($false))
    }
}

if (-not (Test-Path -LiteralPath $playbackTarget -PathType Leaf) -or
    (Get-FileHash -LiteralPath $playbackTarget -Algorithm SHA256).Hash -cne $expectedFixtureHash) {
    throw 'The isolated Phase 4 playback fixture failed verification.'
}
if (-not (Test-Path -LiteralPath $target -PathType Container)) {
    throw "The Phase 4 profile is missing: $TargetProfile"
}
$targetSaves = Join-Path $target 'saves'
if ((Test-Path -LiteralPath $targetSaves -PathType Container) -and
    @(Get-ChildItem -LiteralPath $targetSaves -Force).Count -ne 0) {
    throw 'The Phase 4 profile contains save data.'
}
$targetLines = @(Get-Content -LiteralPath (Join-Path $target 'modlist.txt'))
foreach ($modName in @(
        'Pip-Boy Video Player - Phase 1 Save Guard',
        'Pip-Boy Video Player - Dev',
        $playbackModName)) {
    if (@($targetLines | Where-Object { $_ -eq "+$modName" }).Count -ne 1 -or
        @($targetLines | Where-Object { $_ -eq "-$modName" }).Count -ne 0) {
        throw "The Phase 4 profile does not enable exactly one copy of: $modName"
    }
}
if ($SelectProfile -and
    [IO.File]::ReadAllText($organizerIni) -notmatch
    "(?m)^selected_profile=@ByteArray\($([Text.RegularExpressions.Regex]::Escape($TargetProfile))\)$") {
    throw 'The Phase 4 profile selection failed verification.'
}
Write-Host 'Phase 4 generated-playback profile passed verification.'
