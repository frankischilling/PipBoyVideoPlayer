[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FixtureScript,
    [Parameter(Mandatory)][string]$FixtureDirectory
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "pbvp-phase6-fault-fixtures-$([Guid]::NewGuid().ToString('N'))")
[IO.Directory]::CreateDirectory((Join-Path $temporaryRoot 'mods')) | Out-Null

try {
    & $FixtureScript `
        -InstanceRoot $temporaryRoot `
        -FixtureDirectory $FixtureDirectory
    & $FixtureScript `
        -InstanceRoot $temporaryRoot `
        -FixtureDirectory $FixtureDirectory `
        -VerifyOnly

    $videoRoot = Join-Path $temporaryRoot (
        'mods\Pip-Boy Video Player - Fault Test\NVSE\Plugins\' +
        'PipBoyVideoPlayer\Videos')
    $files = @(Get-ChildItem -LiteralPath $videoRoot -File)
    if ($files.Count -ne 7 -or
        @($files | Where-Object { $_.Name -eq '10 Empty File.mp4' -and $_.Length -eq 0 }).Count -ne 1 -or
        @($files | Where-Object { $_.Name -eq '20 Random Bytes.mp4' -and $_.Length -eq 4096 }).Count -ne 1 -or
        @($files | Where-Object { $_.Name -eq '30 Truncated File.mp4' -and $_.Length -eq 1024 }).Count -ne 1) {
        throw 'The generated fault fixture set is incomplete.'
    }

    $unexpected = Join-Path $videoRoot 'must-remain.txt'
    [IO.File]::WriteAllText(
        $unexpected,
        'preserve this file',
        [Text.UTF8Encoding]::new($false))
    $refused = $false
    try {
        & $FixtureScript `
            -InstanceRoot $temporaryRoot `
            -FixtureDirectory $FixtureDirectory
    } catch {
        $refused = $true
    }
    if (-not $refused -or -not (Test-Path -LiteralPath $unexpected)) {
        throw 'The fault fixture script did not preserve and refuse an unexpected file.'
    }
    Write-Host 'Phase 6 fault fixture tests passed.'
} finally {
    if ((Test-Path -LiteralPath $temporaryRoot) -and
        [IO.Path]::GetFileName($temporaryRoot) -like 'pbvp-phase6-fault-fixtures-*' -and
        (Split-Path -Parent $temporaryRoot) -eq [IO.Path]::GetTempPath().TrimEnd('\')) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
