[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [string]$ManifestPath,
    [string]$LlvmReadObj = 'C:\Program Files\LLVM\bin\llvm-readobj.exe',
    [string]$InventoryPath,
    [string[]]$PrivatePathMarkers = @()
)

$ErrorActionPreference = 'Stop'

if (-not $ManifestPath) {
    $ManifestPath = Join-Path (Split-Path -Parent $PSScriptRoot) 'dependencies\ffmpeg-8.1.2.json'
}

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "FFmpeg manifest is missing: $ManifestPath"
}
if (-not (Test-Path -LiteralPath $RuntimeDirectory -PathType Container)) {
    throw "FFmpeg runtime directory is missing: $RuntimeDirectory"
}
if (-not (Test-Path -LiteralPath $LlvmReadObj -PathType Leaf)) {
    throw "llvm-readobj is missing: $LlvmReadObj"
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$expectedNames = @($manifest.runtime | ForEach-Object { $_.file } | Sort-Object)
$actualNames = @(Get-ChildItem -LiteralPath $RuntimeDirectory -Filter '*.dll' -File |
    ForEach-Object { $_.Name } | Sort-Object)
if (($expectedNames -join "`n") -cne ($actualNames -join "`n")) {
    throw "Unexpected FFmpeg DLL set. Expected $($expectedNames -join ', '); got $($actualNames -join ', ')."
}

$markers = @($PrivatePathMarkers | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
    ForEach-Object { [IO.Path]::GetFullPath($_) } | Sort-Object -Unique)
$inventory = @()
foreach ($entry in $manifest.runtime) {
    $path = Join-Path $RuntimeDirectory $entry.file
    $readOutput = @(& $LlvmReadObj --file-headers --coff-imports $path 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "llvm-readobj failed for $($entry.file): $($readOutput -join [Environment]::NewLine)"
    }
    $readText = $readOutput -join "`n"
    if ($readText -notmatch 'Machine: IMAGE_FILE_MACHINE_I386') {
        throw "$($entry.file) is not an i386 PE image."
    }
    $actualImports = @($readOutput | ForEach-Object {
        if ($_ -match '^\s*Name:\s+(.+?)\s*$') { $Matches[1].ToLowerInvariant() }
    } | Where-Object { $_ } | Sort-Object -Unique)
    $expectedImports = @($entry.imports | ForEach-Object { $_.ToLowerInvariant() } | Sort-Object -Unique)
    if (($expectedImports -join "`n") -cne ($actualImports -join "`n")) {
        throw "$($entry.file) imports changed. Expected $($expectedImports -join ', '); got $($actualImports -join ', ')."
    }

    $bytes = [IO.File]::ReadAllBytes($path)
    $ascii = [Text.Encoding]::ASCII.GetString($bytes)
    $unicode = [Text.Encoding]::Unicode.GetString($bytes)
    foreach ($marker in $markers) {
        $slashMarker = $marker.Replace('\', '/')
        if ($ascii.IndexOf($marker, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $ascii.IndexOf($slashMarker, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $unicode.IndexOf($marker, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $unicode.IndexOf($slashMarker, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            throw "$($entry.file) contains a private build path."
        }
    }

    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if (-not [string]::IsNullOrWhiteSpace($entry.sha256) -and $hash -cne $entry.sha256) {
        throw "$($entry.file) hash mismatch. Expected $($entry.sha256), got $hash."
    }
    $inventory += [ordered]@{
        file = $entry.file
        bytes = $bytes.LongLength
        sha256 = $hash
        machine = 'IMAGE_FILE_MACHINE_I386'
        imports = $actualImports
    }
}

$result = [ordered]@{
    name = $manifest.name
    version = $manifest.version
    license = $manifest.license
    sourceSha256 = $manifest.sourceSha256
    runtime = $inventory
}
if ($InventoryPath) {
    $inventoryParent = Split-Path -Parent $InventoryPath
    if ($inventoryParent) {
        [IO.Directory]::CreateDirectory($inventoryParent) | Out-Null
    }
    [IO.File]::WriteAllText(
        [IO.Path]::GetFullPath($InventoryPath),
        ($result | ConvertTo-Json -Depth 6) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
}

$result | ConvertTo-Json -Depth 6
