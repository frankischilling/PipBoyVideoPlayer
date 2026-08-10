[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$MO2ModsDirectory,
    [string]$BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-vs-recreate-test'),
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$mods = (Resolve-Path -LiteralPath $MO2ModsDirectory).Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$marker = Join-Path $build 'pbvp-recreate-test-enabled.txt'
if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
    throw 'The selected build directory is not armed for the one-shot recreation test.'
}
$target = Join-Path $mods 'Pip-Boy Video Player - Dev'
$resolvedTarget = [IO.Path]::GetFullPath($target)
if (-not $resolvedTarget.StartsWith(
        $mods + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Resolved target escaped the MO2 mods directory.'
}
if (-not (Test-Path -LiteralPath $target -PathType Container)) {
    throw 'Install the normal development mod before installing the recreation test build.'
}

$binary = Join-Path $build "$Configuration\PipBoyVideoPlayer.dll"
$symbols = Join-Path $build "$Configuration\PipBoyVideoPlayer.pdb"
if (-not (Test-Path -LiteralPath $binary -PathType Leaf) -or
    -not (Test-Path -LiteralPath $symbols -PathType Leaf)) {
    throw 'The recreation test build is incomplete.'
}

$pluginDirectory = Join-Path $resolvedTarget 'NVSE\Plugins'
if ($PSCmdlet.ShouldProcess($pluginDirectory, 'Install the one-shot recreation test DLL and PDB')) {
    Copy-Item -LiteralPath $binary -Destination (Join-Path $pluginDirectory 'PipBoyVideoPlayer.dll') -Force
    Copy-Item -LiteralPath $symbols -Destination (Join-Path $pluginDirectory 'PipBoyVideoPlayer.pdb') -Force
}

$installed = Join-Path $pluginDirectory 'PipBoyVideoPlayer.dll'
$installedSymbols = Join-Path $pluginDirectory 'PipBoyVideoPlayer.pdb'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash -ne
        (Get-FileHash -Algorithm SHA256 -LiteralPath $installed).Hash -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $symbols).Hash -ne
        (Get-FileHash -Algorithm SHA256 -LiteralPath $installedSymbols).Hash) {
    throw 'The installed recreation test files do not match the build.'
}
Write-Host 'Installed the hash-matched one-shot recreation test build.'
