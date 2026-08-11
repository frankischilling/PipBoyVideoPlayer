[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$InstanceRoot,
    [string]$SourceProfile = 'PBVP Phase 1 Extended',
    [string]$TargetProfile = 'PBVP Phase 5 Catalog',
    [string]$FixturePath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'tests\fixtures\h264-title-metadata.mp4'),
    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string]$ExpectedFixtureHash = '2124A894CD02C5E0388C63B59B780FDCB90864A690559B8086FBDCD41D7C6593',
    [string]$CatalogModName = 'Pip-Boy Video Player - Catalog Test',
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

function Test-InstanceProcessRunning {
    foreach ($name in @('FalloutNV', 'ModOrganizer')) {
        foreach ($process in Get-Process -Name $name -ErrorAction SilentlyContinue) {
            if ($name -eq 'FalloutNV') {
                return $true
            }
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
    }
    return $false
}

if (-not $VerifyOnly -and (Test-InstanceProcessRunning)) {
    throw 'Close FalloutNV and Mod Organizer before preparing the Phase 5 catalog profile.'
}

$fixture = (Resolve-Path -LiteralPath $FixturePath).Path
$expectedHash = $ExpectedFixtureHash.ToUpperInvariant()
if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -cne $expectedHash) {
    throw 'The Phase 5 catalog fixture hash does not match the canonical generated file.'
}

$japaneseName = -join @(
    [char]0x65E5, [char]0x672C, [char]0x8A9E, '.mp4')
$catalogNames = @(
    'Episode 1.mp4',
    'Episode 2.mp4',
    'Episode 10.mp4',
    "Courier's Cut.mp4",
    "Cafe$([char]0x0301).mp4",
    $japaneseName,
    'A Very Long Generated Catalog Name That Must Stay Inside The Video Page.mp4',
    'Metadata Copy A.mp4',
    'Metadata Copy B.mp4',
    'Z Final Entry.mp4'
)
$catalogMod = Resolve-ChildPath -Root $mods -Name $CatalogModName
$videoRoot = Join-Path $catalogMod 'NVSE\Plugins\PipBoyVideoPlayer\Videos'
$source = Resolve-ChildPath -Root $profiles -Name $SourceProfile
$target = Resolve-ChildPath -Root $profiles -Name $TargetProfile
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "The source test profile is missing: $SourceProfile"
}

$expectedTargets = @{}
foreach ($name in $catalogNames) {
    $path = [IO.Path]::GetFullPath((Join-Path $videoRoot $name))
    if (-not $path.StartsWith(
            [IO.Path]::GetFullPath($videoRoot) + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'A catalog fixture name escaped the Videos directory.'
    }
    $expectedTargets[$path] = $true
}

if (-not $VerifyOnly) {
    if ((Test-Path -LiteralPath $catalogMod) -and
        -not (Test-Path -LiteralPath $catalogMod -PathType Container)) {
        throw 'The Phase 5 catalog mod path is not a directory.'
    }
    if (Test-Path -LiteralPath $catalogMod -PathType Container) {
        $unexpected = @(Get-ChildItem -LiteralPath $catalogMod -Recurse -File | Where-Object {
            -not $expectedTargets.ContainsKey([IO.Path]::GetFullPath($_.FullName))
        })
        if ($unexpected.Count -ne 0) {
            throw 'The Phase 5 catalog mod contains an unexpected file.'
        }
    }
    if ($PSCmdlet.ShouldProcess($videoRoot, 'Install ten generated catalog fixtures')) {
        [IO.Directory]::CreateDirectory($videoRoot) | Out-Null
        foreach ($path in $expectedTargets.Keys) {
            Copy-Item -LiteralPath $fixture -Destination $path -Force
        }
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
        throw 'The Phase 5 profile path is not a directory.'
    }

    $saves = Join-Path $target 'saves'
    if ((Test-Path -LiteralPath $saves -PathType Container) -and
        @(Get-ChildItem -LiteralPath $saves -Force).Count -ne 0) {
        throw 'Refusing to update a Phase 5 profile that contains save data.'
    }
    $modList = Join-Path $target 'modlist.txt'
    if (-not (Test-Path -LiteralPath $modList -PathType Leaf)) {
        throw 'The Phase 5 profile has no modlist.txt.'
    }
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in Get-Content -LiteralPath $modList) {
        $lines.Add($line)
    }
    $comment = '# PBVP Phase 5 catalog test profile. Existing VNV profiles remain unchanged.'
    $insertAt = if ($lines.Count -gt 0 -and
        $lines[0] -eq '# This file was automatically generated by Mod Organizer.') { 1 } else { 0 }
    if (-not $lines.Contains($comment)) {
        $lines.Insert($insertAt, $comment)
        $insertAt++
    }
    $enabledMods = @(
        'Pip-Boy Video Player - Phase 1 Save Guard',
        'Pip-Boy Video Player - Dev',
        $CatalogModName
    )
    $disabledMods = @(
        'Pip-Boy Video Player - Playback Test',
        'Pip-Boy Video Player - Long Playback Test',
        'Pip-Boy Video Player - Media Test',
        'Pip-Boy Video Player - Audio Test'
    )
    foreach ($modName in @($enabledMods + $disabledMods)) {
        for ($index = $lines.Count - 1; $index -ge 0; $index--) {
            if ($lines[$index] -eq "+$modName" -or $lines[$index] -eq "-$modName") {
                $lines.RemoveAt($index)
            }
        }
    }
    foreach ($modName in $enabledMods) {
        $lines.Insert($insertAt, "+$modName")
        $insertAt++
    }
    foreach ($modName in $disabledMods) {
        $lines.Insert($insertAt, "-$modName")
        $insertAt++
    }
    [IO.File]::WriteAllLines($modList, $lines, [Text.UTF8Encoding]::new($false))

    if ($SelectProfile) {
        if (-not (Test-Path -LiteralPath $organizerIni -PathType Leaf)) {
            throw 'ModOrganizer.ini is missing, so the Phase 5 profile cannot be selected.'
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

if (-not (Test-Path -LiteralPath $target -PathType Container)) {
    throw "The Phase 5 profile is missing: $TargetProfile"
}
$installedFiles = @(Get-ChildItem -LiteralPath $videoRoot -File -ErrorAction SilentlyContinue)
if ($installedFiles.Count -ne $catalogNames.Count) {
    throw 'The Phase 5 catalog mod does not contain exactly ten fixtures.'
}
foreach ($path in $expectedTargets.Keys) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $expectedHash) {
        throw 'A Phase 5 catalog fixture failed verification.'
    }
}
$targetSaves = Join-Path $target 'saves'
if ((Test-Path -LiteralPath $targetSaves -PathType Container) -and
    @(Get-ChildItem -LiteralPath $targetSaves -Force).Count -ne 0) {
    throw 'The Phase 5 profile contains save data.'
}
$targetLines = @(Get-Content -LiteralPath (Join-Path $target 'modlist.txt'))
foreach ($modName in @(
        'Pip-Boy Video Player - Phase 1 Save Guard',
        'Pip-Boy Video Player - Dev',
        $CatalogModName)) {
    if (@($targetLines | Where-Object { $_ -eq "+$modName" }).Count -ne 1 -or
        @($targetLines | Where-Object { $_ -eq "-$modName" }).Count -ne 0) {
        throw "The Phase 5 profile does not enable exactly one copy of: $modName"
    }
}
if ($SelectProfile -and
    [IO.File]::ReadAllText($organizerIni) -notmatch
    "(?m)^selected_profile=@ByteArray\($([Text.RegularExpressions.Regex]::Escape($TargetProfile))\)$") {
    throw 'The Phase 5 profile selection failed verification.'
}
Write-Host 'Phase 5 generated-catalog profile passed verification.'
