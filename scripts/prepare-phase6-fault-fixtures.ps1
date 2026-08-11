[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$InstanceRoot,
    [string]$FixtureDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'tests\fixtures'),
    [string]$FaultModName = 'Pip-Boy Video Player - Fault Test',
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
$instance = (Resolve-Path -LiteralPath $InstanceRoot).Path
$mods = Join-Path $instance 'mods'
if (-not (Test-Path -LiteralPath $mods -PathType Container)) {
    throw 'The selected MO2 instance has no mods directory.'
}

function Test-InstanceProcessRunning {
    foreach ($name in @('FalloutNV', 'ModOrganizer')) {
        foreach ($process in Get-Process -Name $name -ErrorAction SilentlyContinue) {
            if ($name -eq 'FalloutNV') {
                return $true
            }
            try {
                if (-not [string]::IsNullOrWhiteSpace($process.Path) -and
                    [IO.Path]::GetFullPath((Split-Path -Parent $process.Path)).TrimEnd(
                        [IO.Path]::DirectorySeparatorChar) -ieq $instance.TrimEnd(
                        [IO.Path]::DirectorySeparatorChar)) {
                    return $true
                }
            } catch {
                continue
            }
        }
    }
    return $false
}

if (-not $VerifyOnly -and (Test-InstanceProcessRunning)) {
    throw 'Close FalloutNV and Mod Organizer before preparing the fault fixtures.'
}

$fixtureRoot = (Resolve-Path -LiteralPath $FixtureDirectory).Path
$sourceFiles = [ordered]@{
    '00 Valid Control.mp4' = [ordered]@{
        source = 'h264-aac-44100-stereo.mp4'
        sha256 = '6549BA517226F8CB0CFB70F3B87489B58AA6ECCFCAB4DB75F808BD219DA1A068'
    }
    '40 Unsupported Video Codec.mp4' = [ordered]@{
        source = 'unsupported-mpeg4-mp3.mp4'
        sha256 = '8FB137AAB7A033BE866607812C5032D14C76EA53657DCFF9B994E2B9706CF990'
    }
    '50 Unsupported Audio Codec.mp4' = [ordered]@{
        source = 'h264-unsupported-mp3.mp4'
        sha256 = '9866BAA718B072C51968E4109A16BB28D2FA5E6C1789C7163CEE74FC7AA76A7B'
    }
    '60 Encrypted Media.mp4' = [ordered]@{
        source = 'encrypted-cenc.mp4'
        sha256 = '8B4436B8DF7717BFA52D55D929FA0738500D6D60D25BA7CE103E9C91890EF615'
    }
}
foreach ($entry in $sourceFiles.GetEnumerator()) {
    $source = Join-Path $fixtureRoot $entry.Value.source
    if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
        (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne
            $entry.Value.sha256) {
        throw "A canonical generated fixture failed verification: $($entry.Value.source)"
    }
}

$faultMod = [IO.Path]::GetFullPath((Join-Path $mods $FaultModName))
if (-not $faultMod.StartsWith(
        [IO.Path]::GetFullPath($mods) + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The fault-test mod path escaped the MO2 mods directory.'
}
$videoRoot = Join-Path $faultMod 'NVSE\Plugins\PipBoyVideoPlayer\Videos'
$expectedNames = @(
    @($sourceFiles.Keys) + @(
        '10 Empty File.mp4',
        '20 Random Bytes.mp4',
        '30 Truncated File.mp4'))

if (-not $VerifyOnly) {
    if ((Test-Path -LiteralPath $faultMod) -and
        -not (Test-Path -LiteralPath $faultMod -PathType Container)) {
        throw 'The Phase 6 fault-test mod path is not a directory.'
    }
    if (Test-Path -LiteralPath $faultMod -PathType Container) {
        $unexpected = @(Get-ChildItem -LiteralPath $faultMod -Recurse -File | Where-Object {
            $_.DirectoryName -ine $videoRoot -or $_.Name -notin $expectedNames
        })
        if ($unexpected.Count -ne 0) {
            throw 'The Phase 6 fault-test mod contains an unexpected file.'
        }
    }
    if ($PSCmdlet.ShouldProcess($videoRoot, 'Install generated fault fixtures')) {
        [IO.Directory]::CreateDirectory($videoRoot) | Out-Null
        foreach ($entry in $sourceFiles.GetEnumerator()) {
            Copy-Item `
                -LiteralPath (Join-Path $fixtureRoot $entry.Value.source) `
                -Destination (Join-Path $videoRoot $entry.Key) `
                -Force
        }
        [IO.File]::WriteAllBytes(
            (Join-Path $videoRoot '10 Empty File.mp4'),
            [byte[]]::new(0))
        $random = [byte[]]::new(4096)
        for ($index = 0; $index -lt $random.Length; $index++) {
            $random[$index] = [byte](($index * 73 + 41) % 256)
        }
        [IO.File]::WriteAllBytes(
            (Join-Path $videoRoot '20 Random Bytes.mp4'),
            $random)
        $control = [IO.File]::ReadAllBytes(
            (Join-Path $fixtureRoot 'h264-aac-44100-stereo.mp4'))
        $truncated = [byte[]]::new(1024)
        [Array]::Copy($control, $truncated, $truncated.Length)
        [IO.File]::WriteAllBytes(
            (Join-Path $videoRoot '30 Truncated File.mp4'),
            $truncated)
    }
}

if (-not (Test-Path -LiteralPath $videoRoot -PathType Container)) {
    throw 'The Phase 6 fault fixture directory is missing.'
}
$installed = @(Get-ChildItem -LiteralPath $videoRoot -File)
if ($installed.Count -ne $expectedNames.Count -or
    @($installed | Where-Object { $_.Name -notin $expectedNames }).Count -ne 0) {
    throw 'The Phase 6 fault fixture inventory is incorrect.'
}
foreach ($entry in $sourceFiles.GetEnumerator()) {
    if ((Get-FileHash -LiteralPath (Join-Path $videoRoot $entry.Key) -Algorithm SHA256).Hash -cne
        $entry.Value.sha256) {
        throw "An installed fault fixture failed verification: $($entry.Key)"
    }
}
$empty = Join-Path $videoRoot '10 Empty File.mp4'
$randomPath = Join-Path $videoRoot '20 Random Bytes.mp4'
$truncatedPath = Join-Path $videoRoot '30 Truncated File.mp4'
if ((Get-Item -LiteralPath $empty).Length -ne 0 -or
    (Get-Item -LiteralPath $randomPath).Length -ne 4096 -or
    (Get-Item -LiteralPath $truncatedPath).Length -ne 1024) {
    throw 'A generated fault fixture has the wrong size.'
}
$randomBytes = [IO.File]::ReadAllBytes($randomPath)
for ($index = 0; $index -lt $randomBytes.Length; $index++) {
    if ($randomBytes[$index] -ne [byte](($index * 73 + 41) % 256)) {
        throw 'The random-byte fault fixture failed verification.'
    }
}
$controlBytes = [IO.File]::ReadAllBytes(
    (Join-Path $fixtureRoot 'h264-aac-44100-stereo.mp4'))
$truncatedBytes = [IO.File]::ReadAllBytes($truncatedPath)
for ($index = 0; $index -lt $truncatedBytes.Length; $index++) {
    if ($truncatedBytes[$index] -ne $controlBytes[$index]) {
        throw 'The truncated fault fixture failed verification.'
    }
}
Write-Host 'Phase 6 generated fault fixtures passed verification.'
