[CmdletBinding()]
param([switch]$Force)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $root '.cache'
$external = Join-Path $root 'external'
[IO.Directory]::CreateDirectory($cache) | Out-Null
[IO.Directory]::CreateDirectory($external) | Out-Null

$dependencies = @(
    @{
        Name = 'xNVSE 6.4.5'
        Uri = 'https://github.com/xNVSE/NVSE/archive/refs/tags/6.4.5.zip'
        Archive = 'xnvse-6.4.5-source.zip'
        Hash = 'A4C03B13BBC810A5452ABC04B1BBDECB7881C766D56CF46B6035F5DF5EFDF343'
        ExpandedName = 'NVSE-6.4.5'
        Destination = 'NVSE-6.4.5'
    }
)

foreach ($dependency in $dependencies) {
    $destination = Join-Path $external $dependency.Destination
    if ((Test-Path -LiteralPath $destination) -and -not $Force) {
        Write-Host "Already present: $destination"
        continue
    }

    $archive = Join-Path $cache $dependency.Archive
    if (-not (Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri $dependency.Uri -OutFile $archive
    }
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($actual -ne $dependency.Hash) {
        throw "$($dependency.Name) source hash mismatch. Expected $($dependency.Hash), got $actual"
    }

    $temporary = Join-Path $cache ("expand-" + $dependency.Destination)
    if (Test-Path -LiteralPath $temporary) {
        Remove-Item -LiteralPath $temporary -Recurse -Force
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $temporary
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    Move-Item -LiteralPath (Join-Path $temporary $dependency.ExpandedName) -Destination $destination
    Write-Host "Installed verified source: $destination"
}
