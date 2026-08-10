[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$AuditScript,
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [Parameter(Mandatory)][string]$ManifestPath,
    [Parameter(Mandatory)][string]$LlvmReadObj,
    [Parameter(Mandatory)][string]$PrivatePathMarker
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = [IO.Path]::Combine(
    [IO.Path]::GetTempPath(),
    "pbvp-ffmpeg-audit-$([Guid]::NewGuid().ToString('N'))")
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null

function Write-Manifest {
    param([Parameter(Mandatory)]$Value)
    $path = Join-Path $temporaryRoot 'manifest.json'
    [IO.File]::WriteAllText(
        $path,
        ($Value | ConvertTo-Json -Depth 8) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
    return $path
}

try {
    $goodOutput = & $AuditScript `
        -RuntimeDirectory $RuntimeDirectory `
        -ManifestPath $ManifestPath `
        -LlvmReadObj $LlvmReadObj `
        -PrivatePathMarkers @($PrivatePathMarker)
    $good = $goodOutput | ConvertFrom-Json
    if ($good.version -cne '8.1.2' -or @($good.runtime).Count -ne 5) {
        throw 'The valid FFmpeg runtime audit returned the wrong inventory.'
    }

    $badHash = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $badHash.runtime[0].sha256 = (('0' * 64) -join '')
    $badHashPath = Write-Manifest $badHash
    $hashRefused = $false
    try {
        & $AuditScript -RuntimeDirectory $RuntimeDirectory -ManifestPath $badHashPath `
            -LlvmReadObj $LlvmReadObj | Out-Null
    } catch {
        if ($_.Exception.Message -notmatch 'hash mismatch') { throw }
        $hashRefused = $true
    }
    if (-not $hashRefused) {
        throw 'The runtime audit accepted a wrong DLL hash.'
    }

    $badImports = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $badImports.runtime[0].imports = @('not-a-real-library.dll')
    $badImportsPath = Write-Manifest $badImports
    $importsRefused = $false
    try {
        & $AuditScript -RuntimeDirectory $RuntimeDirectory -ManifestPath $badImportsPath `
            -LlvmReadObj $LlvmReadObj | Out-Null
    } catch {
        if ($_.Exception.Message -notmatch 'imports changed') { throw }
        $importsRefused = $true
    }
    if (-not $importsRefused) {
        throw 'The runtime audit accepted an unexpected import set.'
    }

    $copiedRuntime = Join-Path $temporaryRoot 'bin'
    [IO.Directory]::CreateDirectory($copiedRuntime) | Out-Null
    Get-ChildItem -LiteralPath $RuntimeDirectory -Filter '*.dll' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $copiedRuntime
    }
    [IO.File]::WriteAllBytes((Join-Path $copiedRuntime 'unexpected.dll'), [byte[]](0))
    $setRefused = $false
    try {
        & $AuditScript -RuntimeDirectory $copiedRuntime -ManifestPath $ManifestPath `
            -LlvmReadObj $LlvmReadObj | Out-Null
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected FFmpeg DLL set') { throw }
        $setRefused = $true
    }
    if (-not $setRefused) {
        throw 'The runtime audit accepted an unexpected DLL.'
    }

    Write-Host 'FFmpeg runtime audit tests passed.'
} finally {
    $resolvedTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
    $systemTemporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTemporaryRoot.StartsWith(
            $systemTemporaryRoot,
            [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolvedTemporaryRoot) -like 'pbvp-ffmpeg-audit-*' -and
        (Test-Path -LiteralPath $resolvedTemporaryRoot)) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
