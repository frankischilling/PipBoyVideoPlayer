[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RuntimeArchive,
    [Parameter(Mandatory)][string]$SymbolsArchive,
    [string]$Version = '0.1.0',
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string[]]$PrivatePathMarkers = @()
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$runtimePath = (Resolve-Path -LiteralPath $RuntimeArchive).Path
$symbolsPath = (Resolve-Path -LiteralPath $SymbolsArchive).Path
if ([IO.Path]::GetFileName($runtimePath) -cne "PipBoyVideoPlayer-$Version.zip") {
    throw 'The runtime archive filename does not match the requested version.'
}
if ([IO.Path]::GetFileName($symbolsPath) -cne "PipBoyVideoPlayer-Symbols-$Version.zip") {
    throw 'The symbols archive filename does not match the requested version.'
}

$manifestPath = Join-Path $root 'dependencies\ffmpeg-8.1.2.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$documentation = @(Get-ChildItem -LiteralPath (Join-Path $root 'docs') -Filter '*.md' -File |
    ForEach-Object { "docs/$($_.Name)" })
$expectedRuntime = @(
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
$expectedRuntime = @($expectedRuntime | Sort-Object -CaseSensitive -Unique)
$expectedSymbols = @('PipBoyVideoPlayer.pdb', 'README.md') | Sort-Object -CaseSensitive
$forbiddenExtensions = @(
    '.mp4', '.mov', '.m4v', '.mkv', '.webm', '.avi',
    '.sav', '.nvse', '.log', '.dmp', '.tmp', '.bak', '.obj', '.lib', '.exe'
)

function Assert-SafeArchive {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string[]]$ExpectedFiles,
        [Parameter(Mandatory)][bool]$AllowPdb
    )

    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        if ($archive.Entries.Count -eq 0 -or $archive.Entries.Count -gt 256) {
            throw "The $Label archive has an invalid entry count."
        }
        $seen = [Collections.Generic.HashSet[string]]::new(
            [StringComparer]::OrdinalIgnoreCase)
        $files = [Collections.Generic.List[string]]::new()
        [UInt64]$totalLength = 0
        $firstTimestamp = $null
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName.Replace('\', '/')
            if ([string]::IsNullOrWhiteSpace($name) -or
                $name.StartsWith('/') -or
                $name -match '^[A-Za-z]:' -or
                $name -match '(^|/)\.\.?(/|$)' -or
                $name.Contains(':')) {
                throw "The $Label archive contains an unsafe entry name: $name"
            }
            $canonical = $name.TrimEnd('/')
            if (-not $seen.Add($canonical)) {
                throw "The $Label archive contains a duplicate entry: $canonical"
            }
            if ($null -eq $firstTimestamp) {
                $firstTimestamp = $entry.LastWriteTime
            } elseif ($entry.LastWriteTime -ne $firstTimestamp) {
                throw "The $Label archive contains nondeterministic entry timestamps."
            }
            if ($name.EndsWith('/')) {
                if ($entry.Length -ne 0) {
                    throw "The $Label archive contains a nonempty directory entry: $name"
                }
                $directoryPrefix = "$canonical/"
                if (-not ($ExpectedFiles | Where-Object {
                        $_.StartsWith($directoryPrefix, [StringComparison]::Ordinal)
                    })) {
                    throw "The $Label archive contains an unexpected directory: $canonical"
                }
                continue
            }

            if ($entry.Length -gt 128MB) {
                throw "The $Label archive contains an oversized file: $name"
            }
            $totalLength += [UInt64]$entry.Length
            if ($totalLength -gt 256MB) {
                throw "The $Label archive expands beyond its size limit."
            }
            $extension = [IO.Path]::GetExtension($name).ToLowerInvariant()
            if ($forbiddenExtensions -contains $extension -or
                (-not $AllowPdb -and $extension -ceq '.pdb')) {
                throw "The $Label archive contains a forbidden file: $name"
            }
            $files.Add($name)

            $stream = $entry.Open()
            $memory = [IO.MemoryStream]::new()
            try {
                $stream.CopyTo($memory)
                $bytes = $memory.ToArray()
            } finally {
                $memory.Dispose()
                $stream.Dispose()
            }
            $views = @(
                [Text.Encoding]::ASCII.GetString($bytes),
                [Text.Encoding]::Unicode.GetString($bytes),
                [Text.Encoding]::BigEndianUnicode.GetString($bytes)
            )
            foreach ($view in $views) {
                if ($view -match '(?i)[A-Z]:[\\/](Users|Modding|Games|Program Files)[\\/]') {
                    throw "The $Label archive contains an absolute local path: $name"
                }
                foreach ($marker in $PrivatePathMarkers) {
                    if ([string]::IsNullOrWhiteSpace($marker)) { continue }
                    foreach ($form in @($marker, $marker.Replace('\', '/'))) {
                        if ($view.IndexOf($form, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                            throw "The $Label archive contains a private path marker: $name"
                        }
                    }
                }
            }
        }

        $actualFiles = @($files | Sort-Object -CaseSensitive)
        $differences = @(Compare-Object `
            -ReferenceObject $ExpectedFiles `
            -DifferenceObject $actualFiles `
            -CaseSensitive)
        if ($differences.Count -ne 0) {
            $detail = $differences | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }
            throw "The $Label archive inventory changed: $($detail -join ', ')"
        }
    } finally {
        $archive.Dispose()
    }
}

Assert-SafeArchive `
    -Path $runtimePath `
    -Label 'runtime' `
    -ExpectedFiles $expectedRuntime `
    -AllowPdb $false
Assert-SafeArchive `
    -Path $symbolsPath `
    -Label 'symbols' `
    -ExpectedFiles $expectedSymbols `
    -AllowPdb $true

Write-Host "Release package audit passed: runtime_files=$($expectedRuntime.Count) symbols_files=$($expectedSymbols.Count)"
