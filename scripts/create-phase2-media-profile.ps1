[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$InstanceRoot,
    [string]$SourceProfile = 'PBVP Phase 1 Extended',
    [string]$TargetProfile = 'PBVP Phase 2 Media',
    [string]$FixturePath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'tests\fixtures\h264-aac-1080p.mp4'),
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
    throw 'Close Mod Organizer before preparing the isolated Phase 2 media profile.'
}

$fixture = (Resolve-Path -LiteralPath $FixturePath).Path
$expectedFixtureHash = '9C1FE63AD54B12B64ED6501AB9AF3B29CE760097503E5AE59B2DBB58C32F4B38'
if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -cne $expectedFixtureHash) {
    throw 'The Phase 2 smoke fixture hash does not match the canonical synthetic file.'
}

$mediaModName = 'Pip-Boy Video Player - Media Test'
$mediaRelativePath = 'NVSE\Plugins\PipBoyVideoPlayer\Videos\PBVP-Phase2-Smoke.mp4'
$mediaMod = Resolve-ChildPath -Root $mods -Name $mediaModName
$mediaTarget = Join-Path $mediaMod $mediaRelativePath
$source = Resolve-ChildPath -Root $profiles -Name $SourceProfile
$target = Resolve-ChildPath -Root $profiles -Name $TargetProfile

if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "The source test profile is missing: $SourceProfile"
}

if (-not $VerifyOnly) {
    if (-not (Test-Path -LiteralPath $mediaMod)) {
        if (-not $PSCmdlet.ShouldProcess($mediaMod, 'Create the isolated synthetic media mod')) {
            return
        }
        [IO.Directory]::CreateDirectory((Split-Path -Parent $mediaTarget)) | Out-Null
    } elseif (-not (Test-Path -LiteralPath $mediaMod -PathType Container)) {
        throw 'The Phase 2 media mod path is not a directory.'
    }
    $unexpectedMediaFiles = @(Get-ChildItem -LiteralPath $mediaMod -Recurse -File | Where-Object {
        $_.FullName -cne $mediaTarget
    })
    if ($unexpectedMediaFiles.Count -ne 0) {
        throw 'The isolated Phase 2 media mod contains an unexpected file.'
    }
    if ($PSCmdlet.ShouldProcess($mediaTarget, 'Install the canonical synthetic MP4 fixture')) {
        [IO.Directory]::CreateDirectory((Split-Path -Parent $mediaTarget)) | Out-Null
        Copy-Item -LiteralPath $fixture -Destination $mediaTarget -Force
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
        throw 'The Phase 2 profile path is not a directory.'
    }

    $saves = Join-Path $target 'saves'
    if ((Test-Path -LiteralPath $saves -PathType Container) -and
        @(Get-ChildItem -LiteralPath $saves -Force).Count -ne 0) {
        throw 'Refusing to update a Phase 2 profile that contains save data.'
    }

    $modList = Join-Path $target 'modlist.txt'
    if (-not (Test-Path -LiteralPath $modList -PathType Leaf)) {
        throw 'The Phase 2 profile has no modlist.txt.'
    }
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in Get-Content -LiteralPath $modList) {
        $lines.Add($line)
    }
    $comment = '# PBVP Phase 2 media smoke profile. Existing VNV profiles remain unchanged.'
    $insertAt = if ($lines.Count -gt 0 -and
        $lines[0] -eq '# This file was automatically generated by Mod Organizer.') { 1 } else { 0 }
    if (-not $lines.Contains($comment)) {
        $lines.Insert($insertAt, $comment)
        $insertAt++
    }
    foreach ($modName in @(
            'Pip-Boy Video Player - Phase 1 Save Guard',
            'Pip-Boy Video Player - Dev',
            $mediaModName)) {
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
            throw 'ModOrganizer.ini is missing, so the Phase 2 profile cannot be selected.'
        }
        $ini = [IO.File]::ReadAllText($organizerIni)
        if ($ini -notmatch '(?m)^selected_profile=.*$') {
            throw 'ModOrganizer.ini has no selected_profile setting.'
        }
        $replacement = "selected_profile=@ByteArray($TargetProfile)"
        $ini = [Text.RegularExpressions.Regex]::Replace(
            $ini, '(?m)^selected_profile=.*$', $replacement, 1)
        [IO.File]::WriteAllText($organizerIni, $ini, [Text.UTF8Encoding]::new($false))
    }
}

if (-not (Test-Path -LiteralPath $mediaTarget -PathType Leaf) -or
    (Get-FileHash -LiteralPath $mediaTarget -Algorithm SHA256).Hash -cne $expectedFixtureHash) {
    throw 'The isolated Phase 2 media fixture failed verification.'
}
if (-not (Test-Path -LiteralPath $target -PathType Container)) {
    throw "The Phase 2 profile is missing: $TargetProfile"
}
$targetSaves = Join-Path $target 'saves'
if ((Test-Path -LiteralPath $targetSaves -PathType Container) -and
    @(Get-ChildItem -LiteralPath $targetSaves -Force).Count -ne 0) {
    throw 'The Phase 2 profile contains save data.'
}
$targetModList = Join-Path $target 'modlist.txt'
$targetLines = @(Get-Content -LiteralPath $targetModList)
foreach ($modName in @(
        'Pip-Boy Video Player - Phase 1 Save Guard',
        'Pip-Boy Video Player - Dev',
        $mediaModName)) {
    if (@($targetLines | Where-Object { $_ -eq "+$modName" }).Count -ne 1 -or
        @($targetLines | Where-Object { $_ -eq "-$modName" }).Count -ne 0) {
        throw "The Phase 2 profile does not enable exactly one copy of: $modName"
    }
}
if ($SelectProfile) {
    if ([IO.File]::ReadAllText($organizerIni) -notmatch
        "(?m)^selected_profile=@ByteArray\($([Text.RegularExpressions.Regex]::Escape($TargetProfile))\)$") {
        throw 'The Phase 2 profile selection failed verification.'
    }
}
Write-Host 'Phase 2 synthetic media profile passed verification.'
