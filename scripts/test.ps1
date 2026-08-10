[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-vs')
)

$ErrorActionPreference = 'Stop'
ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw 'Tests failed.'
}
