[CmdletBinding()]
param(
    [switch]$Force,
    [string]$Msys2Root = 'C:\msys64'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $root '.cache'
$external = Join-Path $root 'external'
[IO.Directory]::CreateDirectory($cache) | Out-Null
[IO.Directory]::CreateDirectory($external) | Out-Null

function Get-ContainedPath {
    param(
        [Parameter(Mandatory)][string]$Parent,
        [Parameter(Mandatory)][string]$Child
    )

    $parentPath = [IO.Path]::GetFullPath($Parent).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $childPath = [IO.Path]::GetFullPath($Child)
    if (-not $childPath.StartsWith($parentPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing path outside $parentPath`: $childPath"
    }
    return $childPath
}

$dependencies = @(
    @{
        Name = 'xNVSE 6.4.5'
        Uri = 'https://github.com/xNVSE/NVSE/archive/refs/tags/6.4.5.zip'
        Archive = 'xnvse-6.4.5-source.zip'
        Hash = 'A4C03B13BBC810A5452ABC04B1BBDECB7881C766D56CF46B6035F5DF5EFDF343'
        Format = 'Zip'
        ExpandedName = 'NVSE-6.4.5'
        Destination = 'NVSE-6.4.5'
    },
    @{
        Name = 'FFmpeg 8.1.2'
        Uri = 'https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz'
        Archive = 'ffmpeg-8.1.2.tar.xz'
        Hash = '464BEB5E7BF0C311E68B45AE2F04E9CC2AF88851ABB4082231742A74D97B524C'
        Format = 'TarXz'
        ExpandedName = 'ffmpeg-8.1.2'
        Destination = 'ffmpeg-8.1.2'
    }
)

foreach ($dependency in $dependencies) {
    $destination = Get-ContainedPath -Parent $external -Child (Join-Path $external $dependency.Destination)
    if ((Test-Path -LiteralPath $destination) -and -not $Force) {
        Write-Host "Already present: $destination"
        continue
    }

    $archive = Get-ContainedPath -Parent $cache -Child (Join-Path $cache $dependency.Archive)
    if (-not (Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri $dependency.Uri -OutFile $archive
    }
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($actual -ne $dependency.Hash) {
        throw "$($dependency.Name) source hash mismatch. Expected $($dependency.Hash), got $actual"
    }

    $temporary = Get-ContainedPath -Parent $cache -Child (
        Join-Path $cache ("expand-" + $dependency.Destination))
    if (Test-Path -LiteralPath $temporary) {
        Remove-Item -LiteralPath $temporary -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($temporary) | Out-Null
    try {
        if ($dependency.Format -eq 'Zip') {
            Expand-Archive -LiteralPath $archive -DestinationPath $temporary
        } elseif ($dependency.Format -eq 'TarXz') {
            $tar = Join-Path $Msys2Root 'usr\bin\tar.exe'
            $xz = Join-Path $Msys2Root 'usr\bin\xz.exe'
            if (-not (Test-Path -LiteralPath $tar -PathType Leaf) -or
                -not (Test-Path -LiteralPath $xz -PathType Leaf)) {
                throw 'FFmpeg extraction requires the MSYS2 tar and xz tools.'
            }
            $savedPath = $env:PATH
            try {
                $env:PATH = (Join-Path $Msys2Root 'usr\bin') + [IO.Path]::PathSeparator + $env:PATH
                & $tar --force-local -xf $archive -C $temporary
                if ($LASTEXITCODE -ne 0) {
                    throw "$($dependency.Name) extraction failed."
                }
            } finally {
                $env:PATH = $savedPath
            }
        } else {
            throw "Unsupported dependency archive format: $($dependency.Format)"
        }

        $expanded = Get-ContainedPath -Parent $temporary -Child (
            Join-Path $temporary $dependency.ExpandedName)
        if (-not (Test-Path -LiteralPath $expanded -PathType Container)) {
            throw "$($dependency.Name) archive did not contain $($dependency.ExpandedName)."
        }
        if (Test-Path -LiteralPath $destination) {
            Remove-Item -LiteralPath $destination -Recurse -Force
        }
        Move-Item -LiteralPath $expanded -Destination $destination
        Write-Host "Installed verified source: $destination"
    } finally {
        if (Test-Path -LiteralPath $temporary) {
            Remove-Item -LiteralPath $temporary -Recurse -Force
        }
    }
}
