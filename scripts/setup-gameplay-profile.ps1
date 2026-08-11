[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$InstanceRoot,
    [Parameter(Mandatory)][string]$RuntimeArchive,
    [string]$SourceProfile = 'Viva New Vegas Extended',
    [string]$TargetProfile = 'Pip-Boy Video Player',
    [string]$PlayerModName = 'Pip-Boy Video Player',
    [string]$MediaModName = 'Pip-Boy Videos',
    [switch]$VerifyOnly,
    [switch]$SelectProfile
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

if ($VerifyOnly -and $SelectProfile) {
    throw 'VerifyOnly cannot select the MO2 profile.'
}

$instance = (Resolve-Path -LiteralPath $InstanceRoot).Path
$archivePath = (Resolve-Path -LiteralPath $RuntimeArchive).Path
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

    if ([string]::IsNullOrWhiteSpace($Name) -or
        $Name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
        $Name.Contains([IO.Path]::DirectorySeparatorChar) -or
        $Name.Contains([IO.Path]::AltDirectorySeparatorChar)) {
        throw "Invalid MO2 child name: $Name"
    }
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

function Get-CheckedArchiveEntries {
    param([Parameter(Mandatory)][string]$Path)

    $requiredFiles = @(
        'Config/PipBoyVideoPlayer.ini',
        'menus/prefabs/PipBoyVideoPlayer/Player.xml',
        'NVSE/Plugins/PipBoyVideoPlayer.dll',
        'NVSE/Plugins/PipBoyVideoPlayer/bin/avcodec-62.dll',
        'NVSE/Plugins/PipBoyVideoPlayer/bin/avformat-62.dll',
        'NVSE/Plugins/PipBoyVideoPlayer/bin/avutil-60.dll',
        'NVSE/Plugins/PipBoyVideoPlayer/bin/swresample-6.dll',
        'NVSE/Plugins/PipBoyVideoPlayer/bin/swscale-9.dll',
        'uio/public/PipBoyVideoPlayer.txt')
    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        if ($archive.Entries.Count -eq 0 -or $archive.Entries.Count -gt 256) {
            throw 'The runtime archive has an invalid entry count.'
        }
        $seen = [Collections.Generic.HashSet[string]]::new(
            [StringComparer]::OrdinalIgnoreCase)
        $files = [Collections.Generic.List[string]]::new()
        [UInt64]$totalLength = 0
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName.Replace('\', '/')
            if ([string]::IsNullOrWhiteSpace($name) -or
                $name.StartsWith('/') -or
                $name -match '^[A-Za-z]:' -or
                $name -match '(^|/)\.\.?(/|$)' -or
                $name.Contains(':')) {
                throw "The runtime archive contains an unsafe entry name: $name"
            }
            $canonical = $name.TrimEnd('/')
            if (-not $seen.Add($canonical)) {
                throw "The runtime archive contains a duplicate entry: $canonical"
            }
            if ($name.EndsWith('/')) {
                if ($entry.Length -ne 0) {
                    throw "The runtime archive contains a nonempty directory entry: $name"
                }
                continue
            }
            if ($entry.Length -gt 128MB) {
                throw "The runtime archive contains an oversized file: $name"
            }
            $totalLength += [UInt64]$entry.Length
            if ($totalLength -gt 256MB) {
                throw 'The runtime archive expands beyond its size limit.'
            }
            if ([IO.Path]::GetExtension($name).ToLowerInvariant() -in
                @('.mp4', '.mov', '.m4v', '.mkv', '.webm', '.avi', '.sav', '.nvse')) {
                throw "The runtime archive contains personal media or save data: $name"
            }
            $files.Add($name)
        }
        foreach ($required in $requiredFiles) {
            if (-not $seen.Contains($required)) {
                throw "The runtime archive is missing a required file: $required"
            }
        }
        return @($files)
    } finally {
        $archive.Dispose()
    }
}

function Install-CheckedArchive {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Destination,
        [Parameter(Mandatory)][string[]]$ExpectedFiles
    )

    if (Test-Path -LiteralPath $Destination) {
        if (-not (Test-Path -LiteralPath $Destination -PathType Container)) {
            throw 'The player mod path exists but is not a directory.'
        }
        foreach ($relative in $ExpectedFiles) {
            $installed = [IO.Path]::GetFullPath((Join-Path $Destination $relative))
            if (-not $installed.StartsWith(
                    $Destination + [IO.Path]::DirectorySeparatorChar,
                    [StringComparison]::OrdinalIgnoreCase) -or
                -not (Test-Path -LiteralPath $installed -PathType Leaf)) {
                throw "The existing player mod is incomplete: $relative"
            }
        }
        return
    }

    $stagingName = ".pbvp-install-$([Guid]::NewGuid().ToString('N'))"
    $staging = Resolve-ChildPath -Root $mods -Name $stagingName
    try {
        [IO.Directory]::CreateDirectory($staging) | Out-Null
        $archive = [IO.Compression.ZipFile]::OpenRead($Path)
        try {
            foreach ($entry in $archive.Entries) {
                $name = $entry.FullName.Replace('\', '/')
                $relative = $name.Replace('/', [IO.Path]::DirectorySeparatorChar)
                $target = [IO.Path]::GetFullPath((Join-Path $staging $relative))
                if (-not $target.StartsWith(
                        $staging + [IO.Path]::DirectorySeparatorChar,
                        [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Archive extraction escaped its staging directory: $name"
                }
                if ($name.EndsWith('/')) {
                    [IO.Directory]::CreateDirectory($target) | Out-Null
                    continue
                }
                [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
                $input = $entry.Open()
                $output = [IO.File]::Open(
                    $target,
                    [IO.FileMode]::CreateNew,
                    [IO.FileAccess]::Write,
                    [IO.FileShare]::None)
                try {
                    $input.CopyTo($output)
                } finally {
                    $output.Dispose()
                    $input.Dispose()
                }
            }
        } finally {
            $archive.Dispose()
        }
        Move-Item -LiteralPath $staging -Destination $Destination
    } finally {
        if ((Test-Path -LiteralPath $staging) -and
            [IO.Path]::GetFileName($staging) -like '.pbvp-install-*' -and
            (Split-Path -Parent $staging) -ieq $mods) {
            Remove-Item -LiteralPath $staging -Recurse -Force
        }
    }
}

if (-not $VerifyOnly -and (Test-InstanceProcessRunning)) {
    throw 'Close FalloutNV and Mod Organizer before setting up the gameplay profile.'
}

$archiveFiles = @(Get-CheckedArchiveEntries -Path $archivePath)
$source = Resolve-ChildPath -Root $profiles -Name $SourceProfile
$target = Resolve-ChildPath -Root $profiles -Name $TargetProfile
$playerMod = Resolve-ChildPath -Root $mods -Name $PlayerModName
$mediaMod = Resolve-ChildPath -Root $mods -Name $MediaModName
$videoDirectory = Join-Path $mediaMod 'NVSE\Plugins\PipBoyVideoPlayer\Videos'
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "The source profile is missing: $SourceProfile"
}

$developmentMods = @(
    'Pip-Boy Video Player - Phase 1 Save Guard',
    'Pip-Boy Video Player - Dev',
    'Pip-Boy Video Player - Catalog Test',
    'Pip-Boy Video Player - Long Playback Test',
    'Pip-Boy Video Player - Fault Test',
    'Pip-Boy Video Player - Playback Test',
    'Pip-Boy Video Player - Media Test',
    'Pip-Boy Video Player - Audio Test')

if (-not $VerifyOnly) {
    if ($PSCmdlet.ShouldProcess($playerMod, 'Install the Pip-Boy Video Player runtime archive')) {
        Install-CheckedArchive `
            -Path $archivePath `
            -Destination $playerMod `
            -ExpectedFiles $archiveFiles
    }
    if ($PSCmdlet.ShouldProcess($videoDirectory, 'Create the personal video directory')) {
        [IO.Directory]::CreateDirectory($videoDirectory) | Out-Null
    }

    if (-not (Test-Path -LiteralPath $target)) {
        if (-not $PSCmdlet.ShouldProcess($target, "Create $TargetProfile without saves")) {
            return
        }
        [IO.Directory]::CreateDirectory($target) | Out-Null
        Get-ChildItem -LiteralPath $source -File | Where-Object {
            $_.Name -notmatch '\.\d{4}_\d{2}_\d{2}_' -and
            $_.Name -notlike '.pbvp-*'
        } | Copy-Item -Destination $target
        [IO.Directory]::CreateDirectory((Join-Path $target 'saves')) | Out-Null
    } elseif (-not (Test-Path -LiteralPath $target -PathType Container)) {
        throw 'The gameplay profile path is not a directory.'
    }

    $modList = Join-Path $target 'modlist.txt'
    if (-not (Test-Path -LiteralPath $modList -PathType Leaf)) {
        throw 'The gameplay profile has no modlist.txt.'
    }
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in Get-Content -LiteralPath $modList) {
        $lines.Add($line)
    }
    $managedMods = @($PlayerModName, $MediaModName) + $developmentMods
    foreach ($modName in $managedMods) {
        for ($index = $lines.Count - 1; $index -ge 0; $index--) {
            if ($lines[$index] -eq "+$modName" -or $lines[$index] -eq "-$modName") {
                $lines.RemoveAt($index)
            }
        }
    }
    $comment = '# Pip-Boy Video Player gameplay profile. Existing VNV profiles remain unchanged.'
    $insertAt = if ($lines.Count -gt 0 -and
        $lines[0] -eq '# This file was automatically generated by Mod Organizer.') { 1 } else { 0 }
    if (-not $lines.Contains($comment)) {
        $lines.Insert($insertAt, $comment)
        $insertAt++
    }
    foreach ($modName in @($PlayerModName, $MediaModName)) {
        $lines.Insert($insertAt, "+$modName")
        $insertAt++
    }
    foreach ($modName in $developmentMods) {
        $lines.Insert($insertAt, "-$modName")
        $insertAt++
    }
    [IO.File]::WriteAllLines($modList, $lines, [Text.UTF8Encoding]::new($false))

    if ($SelectProfile) {
        if (-not (Test-Path -LiteralPath $organizerIni -PathType Leaf)) {
            throw 'ModOrganizer.ini is missing, so the gameplay profile cannot be selected.'
        }
        $ini = [IO.File]::ReadAllText($organizerIni)
        if ($ini -notmatch '(?m)^selected_profile=.*$') {
            throw 'ModOrganizer.ini has no selected_profile setting.'
        }
        $ini = [Text.RegularExpressions.Regex]::Replace(
            $ini,
            '(?m)^selected_profile=.*$',
            "selected_profile=@ByteArray($TargetProfile)",
            1)
        [IO.File]::WriteAllText(
            $organizerIni,
            $ini,
            [Text.UTF8Encoding]::new($false))
    }
}

foreach ($required in @($playerMod, $mediaMod, $target)) {
    if (-not (Test-Path -LiteralPath $required -PathType Container)) {
        throw "The gameplay setup is missing a required directory: $required"
    }
}
foreach ($relative in $archiveFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $playerMod $relative) -PathType Leaf)) {
        throw "The installed player mod is missing a runtime file: $relative"
    }
}
if (-not (Test-Path -LiteralPath $videoDirectory -PathType Container)) {
    throw 'The personal video directory is missing.'
}
$targetModList = Join-Path $target 'modlist.txt'
if (-not (Test-Path -LiteralPath $targetModList -PathType Leaf)) {
    throw 'The gameplay profile has no modlist.txt.'
}
$targetLines = @(Get-Content -LiteralPath $targetModList)
foreach ($modName in @($PlayerModName, $MediaModName)) {
    if (@($targetLines | Where-Object { $_ -eq "+$modName" }).Count -ne 1 -or
        @($targetLines | Where-Object { $_ -eq "-$modName" }).Count -ne 0) {
        throw "The gameplay profile does not enable exactly one copy of: $modName"
    }
}
foreach ($modName in $developmentMods) {
    if (@($targetLines | Where-Object { $_ -eq "-$modName" }).Count -ne 1 -or
        @($targetLines | Where-Object { $_ -eq "+$modName" }).Count -ne 0) {
        throw "The gameplay profile does not disable exactly one copy of: $modName"
    }
}
if ($SelectProfile -and
    [IO.File]::ReadAllText($organizerIni) -notmatch
    "(?m)^selected_profile=@ByteArray\($([Text.RegularExpressions.Regex]::Escape($TargetProfile))\)$") {
    throw 'The gameplay profile selection failed verification.'
}

Write-Host "Gameplay profile ready: $TargetProfile"
Write-Host "Add MP4 files here: $videoDirectory"
