[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CheckerPath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    "pbvp-phase6-install-test-$([Guid]::NewGuid().ToString('N'))")
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null

function New-FixtureArchive {
    param(
        [Parameter(Mandatory)][string]$Path,
        [switch]$UnexpectedPlugin,
        [switch]$Traversal
    )
    $archive = [IO.Compression.ZipFile]::Open(
        $Path,
        [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entries = @(
            'Config/PipBoyVideoPlayer.ini',
            'menus/prefabs/PipBoyVideoPlayer/Player.xml',
            'NVSE/Plugins/PipBoyVideoPlayer.dll',
            'uio/public/PipBoyVideoPlayer.txt')
        if ($UnexpectedPlugin) { $entries += 'Unexpected.esp' }
        if ($Traversal) { $entries += '../escaped.txt' }
        foreach ($name in $entries) {
            $entry = $archive.CreateEntry($name)
            $writer = [IO.StreamWriter]::new(
                $entry.Open(),
                [Text.UTF8Encoding]::new($false))
            try {
                $writer.Write('fixture')
            } finally {
                $writer.Dispose()
            }
        }
    } finally {
        $archive.Dispose()
    }
}

try {
    $good = Join-Path $temporaryRoot 'good.zip'
    New-FixtureArchive -Path $good
    & $CheckerPath -RuntimeArchive $good

    foreach ($case in @(
            @{ Name = 'plugin'; Arguments = @{ UnexpectedPlugin = $true } },
            @{ Name = 'traversal'; Arguments = @{ Traversal = $true } })) {
        $path = Join-Path $temporaryRoot "$($case.Name).zip"
        $arguments = $case.Arguments
        New-FixtureArchive -Path $path @arguments
        $rejected = $false
        try {
            & $CheckerPath -RuntimeArchive $path
        } catch {
            $rejected = $true
        }
        if (-not $rejected) {
            throw "The install checker accepted the $($case.Name) fixture."
        }
    }
    Write-Host 'Phase 6 install and removal checker tests passed.'
} finally {
    if ((Test-Path -LiteralPath $temporaryRoot) -and
        [IO.Path]::GetFileName($temporaryRoot) -like 'pbvp-phase6-install-test-*' -and
        (Split-Path -Parent $temporaryRoot) -eq [IO.Path]::GetTempPath().TrimEnd('\')) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
