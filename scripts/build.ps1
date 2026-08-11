[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-vs'),
    [ValidateRange(1, 64)][int]$Jobs = 2
)

$ErrorActionPreference = 'Stop'
$previousNodeReuse = $env:MSBUILDDISABLENODEREUSE
try {
    $env:MSBUILDDISABLENODEREUSE = '1'
    cmake --build $BuildDirectory --config $Configuration --parallel $Jobs
    $buildExitCode = $LASTEXITCODE
} finally {
    if ($null -eq $previousNodeReuse) {
        Remove-Item Env:MSBUILDDISABLENODEREUSE -ErrorAction SilentlyContinue
    } else {
        $env:MSBUILDDISABLENODEREUSE = $previousNodeReuse
    }
}
if ($buildExitCode -ne 0) {
    throw 'Build failed.'
}
