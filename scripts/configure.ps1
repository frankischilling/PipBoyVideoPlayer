[CmdletBinding()]
param(
    [ValidateSet('host','plugin')][string]$Target = 'plugin',
    [string]$BuildDirectory,
    [string]$VisualStudioGenerator
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $root $(if ($Target -eq 'plugin') { 'build-vs' } else { 'build-host' })
}

if ($Target -eq 'plugin') {
    if (-not (Test-Path -LiteralPath (Join-Path $root 'external\NVSE-6.4.5\nvse\nvse\PluginAPI.h')) -or
        -not (Test-Path -LiteralPath (Join-Path $root 'external\minhook-1.3.4\CMakeLists.txt'))) {
        throw 'Verified dependencies are missing. Run scripts\fetch-dependencies.ps1 first.'
    }
    if (-not $VisualStudioGenerator) {
        $vs2022Root = 'C:\Program Files\Microsoft Visual Studio\2022'
        $hasVs2022 = @(Get-ChildItem -LiteralPath $vs2022Root -Directory -ErrorAction SilentlyContinue | Where-Object {
            Test-Path -LiteralPath (Join-Path $_.FullName 'Common7\IDE\devenv.exe')
        }).Count -gt 0
        $VisualStudioGenerator = if ($hasVs2022) { 'Visual Studio 17 2022' } else { 'Visual Studio 18 2026' }
    }
    cmake -S $root -B $BuildDirectory -G $VisualStudioGenerator -A Win32
} else {
    cmake -S $root -B $BuildDirectory -G Ninja -DPBVP_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Debug
}
if ($LASTEXITCODE -ne 0) {
    throw 'CMake configure failed.'
}
