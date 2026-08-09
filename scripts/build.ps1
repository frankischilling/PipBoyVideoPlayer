[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-vs')
)

$ErrorActionPreference = 'Stop'
cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) {
    throw 'Build failed.'
}
