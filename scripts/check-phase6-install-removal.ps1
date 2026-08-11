[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RuntimeArchive
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$archivePath = (Resolve-Path -LiteralPath $RuntimeArchive).Path
$temporaryParent = [IO.Path]::GetTempPath().TrimEnd(
    [IO.Path]::DirectorySeparatorChar,
    [IO.Path]::AltDirectorySeparatorChar)
$temporaryRoot = Join-Path $temporaryParent (
    "pbvp-phase6-install-$([Guid]::NewGuid().ToString('N'))")
$modsRoot = Join-Path $temporaryRoot 'mods'
$profileSaves = Join-Path $temporaryRoot 'profiles\Verification\saves'
$modRoot = Join-Path $modsRoot 'Pip-Boy Video Player'
$sentinel = Join-Path $profileSaves 'must-remain.fos'

function Install-CheckedArchive {
    if (Test-Path -LiteralPath $modRoot) {
        throw 'The isolated install target already exists.'
    }
    [IO.Directory]::CreateDirectory($modRoot) | Out-Null
    $rootPrefix = [IO.Path]::GetFullPath($modRoot) +
        [IO.Path]::DirectorySeparatorChar
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName.Replace('/', '\')
            if ([string]::IsNullOrWhiteSpace($name) -or
                $name.StartsWith('\') -or $name -match '^[A-Za-z]:' -or
                $name -match '(^|\\)\.\.?($|\\)' -or $name.Contains(':')) {
                throw "The runtime archive contains an unsafe entry: $name"
            }
            $canonical = $name.TrimEnd('\')
            if (-not $seen.Add($canonical)) {
                throw "The runtime archive contains a duplicate entry: $canonical"
            }
            $destination = [IO.Path]::GetFullPath((Join-Path $modRoot $canonical))
            if (-not $destination.StartsWith(
                    $rootPrefix,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw "The runtime archive entry escaped the mod root: $name"
            }
            if ($name.EndsWith('\')) {
                [IO.Directory]::CreateDirectory($destination) | Out-Null
                continue
            }
            [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) |
                Out-Null
            $input = $entry.Open()
            $output = [IO.File]::Open(
                $destination,
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

    foreach ($required in @(
            'Config\PipBoyVideoPlayer.ini',
            'menus\prefabs\PipBoyVideoPlayer\Player.xml',
            'NVSE\Plugins\PipBoyVideoPlayer.dll',
            'uio\public\PipBoyVideoPlayer.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $modRoot $required) -PathType Leaf)) {
            throw "The isolated installation is missing: $required"
        }
    }
    $unexpected = @(Get-ChildItem -LiteralPath $modRoot -Recurse -File |
        Where-Object {
            $_.Extension -in @('.esp', '.esm', '.esl', '.fos', '.nvse', '.mp4') -or
            $_.Name -ieq 'plugins.txt'
        })
    if ($unexpected.Count -ne 0) {
        throw 'The isolated installation contains a plugin, save, or media file.'
    }
}

try {
    [IO.Directory]::CreateDirectory($modsRoot) | Out-Null
    [IO.Directory]::CreateDirectory($profileSaves) | Out-Null
    [IO.File]::WriteAllText(
        $sentinel,
        'PBVP save-safety sentinel',
        [Text.UTF8Encoding]::new($false))
    $sentinelHash = (Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash

    Install-CheckedArchive
    Remove-Item -LiteralPath $modRoot -Recurse -Force
    if (-not (Test-Path -LiteralPath $sentinel -PathType Leaf) -or
        (Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash -cne
            $sentinelHash) {
        throw 'Removing the isolated mod changed the save sentinel.'
    }

    Install-CheckedArchive
    if (-not (Test-Path -LiteralPath $sentinel -PathType Leaf) -or
        (Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash -cne
            $sentinelHash) {
        throw 'Reinstalling the isolated mod changed the save sentinel.'
    }
    Write-Host 'Phase 6 isolated install, removal, reinstallation, and save-safety check passed.'
} finally {
    if ((Test-Path -LiteralPath $temporaryRoot) -and
        [IO.Path]::GetFileName($temporaryRoot) -like 'pbvp-phase6-install-*' -and
        (Split-Path -Parent $temporaryRoot) -eq $temporaryParent) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
