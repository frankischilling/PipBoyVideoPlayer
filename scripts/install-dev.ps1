[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$MO2ModsDirectory,
    [string]$StageDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'stage\PipBoyVideoPlayer')
)

$ErrorActionPreference = 'Stop'
$mods = (Resolve-Path -LiteralPath $MO2ModsDirectory).Path
$stage = (Resolve-Path -LiteralPath $StageDirectory).Path
$target = Join-Path $mods 'Pip-Boy Video Player - Dev'
$resolvedTarget = [IO.Path]::GetFullPath($target)
if (-not $resolvedTarget.StartsWith($mods + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Resolved target escaped the MO2 mods directory.'
}
if (Test-Path -LiteralPath $target) {
    throw "Development target already exists: $target. Remove it explicitly before reinstalling."
}
if ($PSCmdlet.ShouldProcess($target, 'Copy staged Pip-Boy Video Player development mod')) {
    Copy-Item -LiteralPath $stage -Destination $target -Recurse
}
