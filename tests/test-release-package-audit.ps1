[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath,
    [Parameter(Mandatory)][string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$checker = (Resolve-Path -LiteralPath $CheckerPath).Path
$manifest = Get-Content -LiteralPath (
    Join-Path $root 'dependencies\ffmpeg-8.1.2.json') -Raw | ConvertFrom-Json
$documentation = @(Get-ChildItem -LiteralPath (Join-Path $root 'docs') -Filter '*.md' -File |
    ForEach-Object { "docs/$($_.Name)" })
$runtimeFiles = @(
    'CHANGELOG.md'
    'Config/PipBoyVideoPlayer.ini'
    'menus/prefabs/PipBoyVideoPlayer/Player.xml'
    'NVSE/Plugins/PipBoyVideoPlayer.dll'
    'README.md'
    'textures/Interface/PipBoyVideoPlayer/Surface.dds'
    'THIRD_PARTY_NOTICES.md'
    'uio/public/PipBoyVideoPlayer.txt'
    'LICENSES/FFmpeg/build-manifest.json'
    'LICENSES/winpthreads/COPYING'
) + $documentation + @($manifest.licenseFiles | ForEach-Object {
    "LICENSES/FFmpeg/$($_.file)"
}) + @($manifest.runtime | ForEach-Object {
    "NVSE/Plugins/PipBoyVideoPlayer/bin/$($_.file)"
})
$symbolsFiles = @('PipBoyVideoPlayer.pdb', 'README.md')
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "pbvp-package-audit-test-$([Guid]::NewGuid().ToString('N'))")
$fixedTime = [DateTime]::SpecifyKind(
    [DateTime]'2026-08-09T00:00:00', [DateTimeKind]::Utc)

function New-FixtureTree {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string[]]$Files
    )
    [IO.Directory]::CreateDirectory($Path) | Out-Null
    foreach ($relative in $Files) {
        $target = Join-Path $Path $relative
        [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
        [IO.File]::WriteAllText($target, "fixture $relative")
    }
    Get-ChildItem -LiteralPath $Path -Recurse -Force |
        ForEach-Object { $_.LastWriteTimeUtc = $fixedTime }
}

function New-Archive {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Path
    )
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Force
    }
    [IO.Compression.ZipFile]::CreateFromDirectory(
        $Source, $Path, [IO.Compression.CompressionLevel]::Optimal, $false)
}

function Assert-Rejected {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action
    )
    try {
        & $Action
    } catch {
        return
    }
    throw "The release package audit accepted the $Name fixture."
}

try {
    $runtimeTree = Join-Path $temporaryRoot 'runtime'
    $symbolsTree = Join-Path $temporaryRoot 'symbols'
    [IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
    New-FixtureTree -Path $runtimeTree -Files $runtimeFiles
    New-FixtureTree -Path $symbolsTree -Files $symbolsFiles
    $runtimeArchive = Join-Path $temporaryRoot 'PipBoyVideoPlayer-0.1.0.zip'
    $symbolsArchive = Join-Path $temporaryRoot 'PipBoyVideoPlayer-Symbols-0.1.0.zip'
    New-Archive -Source $runtimeTree -Path $runtimeArchive
    New-Archive -Source $symbolsTree -Path $symbolsArchive

    & $checker `
        -RuntimeArchive $runtimeArchive `
        -SymbolsArchive $symbolsArchive `
        -RepositoryRoot $root | Out-Null

    $extraDll = Join-Path $runtimeTree 'NVSE\Plugins\unexpected.dll'
    [IO.File]::WriteAllText($extraDll, 'unexpected')
    (Get-Item -LiteralPath $extraDll).LastWriteTimeUtc = $fixedTime
    New-Archive -Source $runtimeTree -Path $runtimeArchive
    Assert-Rejected -Name 'unexpected DLL' -Action {
        & $checker -RuntimeArchive $runtimeArchive -SymbolsArchive $symbolsArchive `
            -RepositoryRoot $root | Out-Null
    }
    Remove-Item -LiteralPath $extraDll -Force

    $media = Join-Path $runtimeTree 'NVSE\Plugins\PipBoyVideoPlayer\Videos\private.mp4'
    [IO.Directory]::CreateDirectory((Split-Path -Parent $media)) | Out-Null
    [IO.File]::WriteAllText($media, 'personal media')
    (Get-Item -LiteralPath $media).LastWriteTimeUtc = $fixedTime
    New-Archive -Source $runtimeTree -Path $runtimeArchive
    Assert-Rejected -Name 'personal media' -Action {
        & $checker -RuntimeArchive $runtimeArchive -SymbolsArchive $symbolsArchive `
            -RepositoryRoot $root | Out-Null
    }
    Remove-Item -LiteralPath (Split-Path -Parent $media) -Recurse -Force

    $readme = Join-Path $runtimeTree 'README.md'
    $originalReadme = [IO.File]::ReadAllText($readme)
    [IO.File]::WriteAllText($readme, 'C:\Users\private\source')
    (Get-Item -LiteralPath $readme).LastWriteTimeUtc = $fixedTime
    New-Archive -Source $runtimeTree -Path $runtimeArchive
    Assert-Rejected -Name 'absolute path' -Action {
        & $checker -RuntimeArchive $runtimeArchive -SymbolsArchive $symbolsArchive `
            -RepositoryRoot $root | Out-Null
    }
    [IO.File]::WriteAllText($readme, $originalReadme)
    (Get-Item -LiteralPath $readme).LastWriteTimeUtc = $fixedTime

    Remove-Item -LiteralPath $readme -Force
    New-Archive -Source $runtimeTree -Path $runtimeArchive
    Assert-Rejected -Name 'missing required file' -Action {
        & $checker -RuntimeArchive $runtimeArchive -SymbolsArchive $symbolsArchive `
            -RepositoryRoot $root | Out-Null
    }
    [IO.File]::WriteAllText($readme, $originalReadme)
    (Get-Item -LiteralPath $readme).LastWriteTimeUtc = $fixedTime
    New-Archive -Source $runtimeTree -Path $runtimeArchive

    $zip = [IO.Compression.ZipFile]::Open(
        $runtimeArchive, [IO.Compression.ZipArchiveMode]::Update)
    try {
        $entry = $zip.CreateEntry('../private.log')
        $entry.LastWriteTime = [DateTimeOffset]$fixedTime
    } finally {
        $zip.Dispose()
    }
    Assert-Rejected -Name 'path traversal' -Action {
        & $checker -RuntimeArchive $runtimeArchive -SymbolsArchive $symbolsArchive `
            -RepositoryRoot $root | Out-Null
    }

    Write-Host 'Release package audit tests passed.'
} finally {
    if ((Test-Path -LiteralPath $temporaryRoot) -and
        [IO.Path]::GetFileName($temporaryRoot) -like 'pbvp-package-audit-test-*' -and
        (Split-Path -Parent $temporaryRoot) -eq [IO.Path]::GetTempPath().TrimEnd('\')) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
