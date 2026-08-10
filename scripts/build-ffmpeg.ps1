[CmdletBinding()]
param(
    [switch]$Clean,
    [ValidateRange(1, 64)][int]$Jobs = [Environment]::ProcessorCount,
    [string]$Msys2Root = 'C:\msys64',
    [string]$LlvmReadObj = 'C:\Program Files\LLVM\bin\llvm-readobj.exe'
)

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$manifestPath = Join-Path $root 'dependencies\ffmpeg-8.1.2.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$sourceDirectory = Join-Path $root "external\ffmpeg-$($manifest.version)"
$buildDirectory = Join-Path $root 'build-ffmpeg'
$installDirectory = Join-Path $root "external\ffmpeg-$($manifest.version)-i686"
$runtimeDirectory = Join-Path $installDirectory 'bin'

function Assert-ContainedDirectory {
    param([Parameter(Mandatory)][string]$Path)
    $candidate = [IO.Path]::GetFullPath($Path)
    $rootPrefix = $root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $candidate.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        $candidate -ceq $root) {
        throw "Refusing build path outside the repository: $candidate"
    }
    return $candidate
}

$buildDirectory = Assert-ContainedDirectory $buildDirectory
$installDirectory = Assert-ContainedDirectory $installDirectory
if (-not (Test-Path -LiteralPath (Join-Path $sourceDirectory 'configure') -PathType Leaf)) {
    throw 'Verified FFmpeg source is missing. Run scripts\fetch-dependencies.ps1 first.'
}

$tools = [ordered]@{
    bash = Join-Path $Msys2Root 'usr\bin\bash.exe'
    gcc = Join-Path $Msys2Root 'mingw32\bin\gcc.exe'
    nasm = Join-Path $Msys2Root 'mingw32\bin\nasm.exe'
    make = Join-Path $Msys2Root 'usr\bin\make.exe'
    pkgconf = Join-Path $Msys2Root 'mingw32\bin\pkgconf.exe'
    llvm = $LlvmReadObj
}
foreach ($tool in $tools.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $tool.Value -PathType Leaf)) {
        throw "Required $($tool.Key) tool is missing: $($tool.Value)"
    }
}

$versionChecks = @(
    @{ Name = 'gcc'; Actual = (& $tools.gcc --version | Select-Object -First 1); Expected = $manifest.toolVersions.gcc },
    @{ Name = 'nasm'; Actual = (& $tools.nasm -v); Expected = $manifest.toolVersions.nasm },
    @{ Name = 'make'; Actual = (& $tools.make --version | Select-Object -First 1); Expected = $manifest.toolVersions.make },
    @{ Name = 'pkgconf'; Actual = (& $tools.pkgconf --version); Expected = $manifest.toolVersions.pkgconf },
    @{ Name = 'llvm'; Actual = ((& $tools.llvm --version) -join ' '); Expected = $manifest.toolVersions.llvm }
)
foreach ($check in $versionChecks) {
    if ([string]$check.Actual -notmatch [regex]::Escape([string]$check.Expected)) {
        throw "$($check.Name) version mismatch. Expected $($check.Expected); got $($check.Actual)."
    }
}

$winpthreadsArchive = Join-Path $Msys2Root 'mingw32\lib\libwinpthread.a'
$winpthreadsLicense = Join-Path $Msys2Root 'mingw32\share\licenses\winpthreads\COPYING'
foreach ($path in @($winpthreadsArchive, $winpthreadsLicense)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Pinned winpthreads file is missing: $path"
    }
}
if ((Get-FileHash -LiteralPath $winpthreadsArchive -Algorithm SHA256).Hash -cne
        $manifest.staticSupport.archiveSha256 -or
    (Get-FileHash -LiteralPath $winpthreadsLicense -Algorithm SHA256).Hash -cne
        $manifest.staticSupport.licenseFileSha256) {
    throw 'The installed winpthreads support library or license does not match the manifest.'
}

if ($Clean) {
    foreach ($directory in @($buildDirectory, $installDirectory)) {
        if (Test-Path -LiteralPath $directory) {
            Remove-Item -LiteralPath $directory -Recurse -Force
        }
    }
}
[IO.Directory]::CreateDirectory($buildDirectory) | Out-Null

if ($root -notmatch '^([A-Za-z]):\\(.*)$') {
    throw 'The FFmpeg build requires a drive-qualified Windows repository path.'
}
$drive = $Matches[1].ToLowerInvariant()
$tail = $Matches[2].Replace('\', '/')
$msysRootPath = "/$drive/$tail"
if ($msysRootPath -match '\s') {
    throw 'The deterministic FFmpeg build requires a repository path without spaces.'
}
$singleQuote = "'"
$quotedRoot = $singleQuote + $msysRootPath.Replace($singleQuote, $singleQuote + '"' + $singleQuote + '"' + $singleQuote) + $singleQuote
$configure = @(
    "../external/ffmpeg-$($manifest.version)/configure",
    "--prefix=../external/ffmpeg-$($manifest.version)-i686"
) + @($manifest.configureArguments)
$command = @(
    'set -e',
    'export PATH=/mingw32/bin:/usr/bin',
    "export SOURCE_DATE_EPOCH=$($manifest.sourceDateEpoch)",
    "export CFLAGS=-ffile-prefix-map=$msysRootPath=pbvp",
    "cd $quotedRoot",
    'mkdir -p build-ffmpeg',
    'cd build-ffmpeg',
    ($configure -join ' '),
    "make -j$Jobs",
    'make install'
) -join '; '

& $tools.bash -lc $command
if ($LASTEXITCODE -ne 0) {
    throw 'FFmpeg build failed.'
}

$inventoryPath = Join-Path $buildDirectory 'ffmpeg-runtime-inventory.json'
& (Join-Path $PSScriptRoot 'audit-ffmpeg-runtime.ps1') `
    -RuntimeDirectory $runtimeDirectory `
    -ManifestPath $manifestPath `
    -LlvmReadObj $LlvmReadObj `
    -InventoryPath $inventoryPath `
    -PrivatePathMarkers @($root, $Msys2Root, $env:USERPROFILE) | Out-Host
Write-Host "Verified FFmpeg runtime: $runtimeDirectory"
Write-Host "Runtime inventory: $inventoryPath"
