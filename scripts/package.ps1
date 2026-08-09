[CmdletBinding()]
param(
    [string]$Version = '0.1.0',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

function Write-PbvpSurfaceDds {
    param([Parameter(Mandatory)][string]$Path)

    $width = 256
    $height = 256
    $directory = Split-Path -Parent $Path
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([UInt32]0x20534444)
        $writer.Write([UInt32]124)
        $writer.Write([UInt32]0x0000100F)
        $writer.Write([UInt32]$height)
        $writer.Write([UInt32]$width)
        $writer.Write([UInt32]($width * 4))
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]0)
        for ($index = 0; $index -lt 11; $index++) { $writer.Write([UInt32]0) }
        $writer.Write([UInt32]32)
        $writer.Write([UInt32]0x41)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]32)
        $writer.Write([UInt32]0x00FF0000)
        $writer.Write([UInt32]0x0000FF00)
        $writer.Write([UInt32]0x000000FF)
        $writer.Write([UInt32]0xFF000000L)
        $writer.Write([UInt32]0x1000)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]0)
        for ($pixel = 0; $pixel -lt ($width * $height); $pixel++) {
            $writer.Write([UInt32]0xFF300030L)
        }
    } finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

$root = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $root "build-vs\$Configuration\PipBoyVideoPlayer.dll"
$pdb = Join-Path $root "build-vs\$Configuration\PipBoyVideoPlayer.pdb"
if (-not (Test-Path -LiteralPath $binary)) { throw "Missing build output: $binary" }
if (-not (Test-Path -LiteralPath $pdb)) { throw "Missing symbols: $pdb" }

$stageRoot = [IO.Path]::GetFullPath((Join-Path $root 'stage'))
$stage = Join-Path $stageRoot 'PipBoyVideoPlayer'
$symbols = Join-Path $stageRoot 'PipBoyVideoPlayer-Symbols'
foreach ($candidate in @($stage, $symbols)) {
    $resolved = [IO.Path]::GetFullPath($candidate)
    if (-not $resolved.StartsWith($stageRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe staging path: $resolved"
    }
}
foreach ($candidate in @($stage, $symbols)) {
    if (Test-Path -LiteralPath $candidate) { Remove-Item -LiteralPath $candidate -Recurse -Force }
    [IO.Directory]::CreateDirectory($candidate) | Out-Null
}

Copy-Item -Path (Join-Path $root 'data\*') -Destination $stage -Recurse -Force
$surfacePath = Join-Path $stage 'textures\Interface\PipBoyVideoPlayer\Surface.dds'
Write-PbvpSurfaceDds -Path $surfacePath
$pluginDirectory = Join-Path $stage 'NVSE\Plugins'
[IO.Directory]::CreateDirectory($pluginDirectory) | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $pluginDirectory 'PipBoyVideoPlayer.dll')
Copy-Item -LiteralPath $pdb -Destination (Join-Path $pluginDirectory 'PipBoyVideoPlayer.pdb')
foreach ($document in @('README.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $root $document) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $root 'licenses') -Destination $stage -Recurse
Copy-Item -LiteralPath $pdb -Destination $symbols
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $symbols

$unexpected = Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object {
    $_.Extension -in @('.mp4', '.mov', '.m4v', '.mkv', '.webm', '.log', '.dmp')
}
if ($unexpected) {
    throw "Staging contains forbidden files: $($unexpected.FullName -join ', ')"
}

$dist = Join-Path $root 'dist'
[IO.Directory]::CreateDirectory($dist) | Out-Null
$runtimeArchive = Join-Path $dist "PipBoyVideoPlayer-$Version.zip"
$symbolsArchive = Join-Path $dist "PipBoyVideoPlayer-Symbols-$Version.zip"
$fixedTime = [DateTime]::SpecifyKind([DateTime]'2026-08-09T00:00:00', [DateTimeKind]::Utc)
Get-ChildItem -LiteralPath $stageRoot -Recurse -Force | ForEach-Object { $_.LastWriteTimeUtc = $fixedTime }
$sevenZip = (Get-Command 7z -ErrorAction Stop).Source
foreach ($archive in @($runtimeArchive, $symbolsArchive)) {
    if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
}
Push-Location $stage
try { & $sevenZip a -tzip -mx=9 -mtc=off -mta=off $runtimeArchive '.\*' | Out-Null } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw 'Runtime archive creation failed.' }
Push-Location $symbols
try { & $sevenZip a -tzip -mx=9 -mtc=off -mta=off $symbolsArchive '.\*' | Out-Null } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw 'Symbols archive creation failed.' }
Write-Host $runtimeArchive
Write-Host $symbolsArchive
