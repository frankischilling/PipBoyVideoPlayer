[CmdletBinding()]
param(
    [string]$FfmpegPath = 'ffmpeg',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\tests\fixtures')
)

$ErrorActionPreference = 'Stop'
$expectedGeneratorHash = '74DB6C184A03DBA2BDFE23E1A1F41CF5A8385BC1DE6A7A1B26DB1DC541ABEF93'
$expectedFiles = [ordered]@{
    'h264-aac-44100-stereo.mp4' = '6549BA517226F8CB0CFB70F3B87489B58AA6ECCFCAB4DB75F808BD219DA1A068'
    'h264-aac-rotate90.mp4' = '50A6C659DF9A93D326CB57532900FB45125ECE026CA295FEA4383199713F5995'
    'h264-vfr-silent.mp4' = '916E9074593A4FEC98C9BAA9324DA5D47B2A0E6AD36DDEDB8C4E572BE22DC1EE'
    'unsupported-mpeg4-mp3.mp4' = '8FB137AAB7A033BE866607812C5032D14C76EA53657DCFF9B994E2B9706CF990'
}

$command = Get-Command $FfmpegPath -ErrorAction Stop
$generator = if ($command.CommandType -eq 'Application') {
    $command.Source
} else {
    throw "FFmpeg must resolve to an executable: $FfmpegPath"
}
$generatorItem = Get-Item -LiteralPath $generator
if ($generatorItem.LinkType -eq 'SymbolicLink') {
    $generator = [string]$generatorItem.Target
}
$generatorHash = (Get-FileHash -LiteralPath $generator -Algorithm SHA256).Hash
if ($generatorHash -ne $expectedGeneratorHash) {
    throw "Fixture generator hash mismatch. Expected $expectedGeneratorHash, found $generatorHash."
}

$output = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $output | Out-Null

function Invoke-FixtureGenerator {
    param([string[]]$Arguments)

    & $generator @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg fixture generation failed with exit code $LASTEXITCODE."
    }
}

$base = Join-Path $output 'h264-aac-44100-stereo.mp4'
$rotated = Join-Path $output 'h264-aac-rotate90.mp4'
$vfr = Join-Path $output 'h264-vfr-silent.mp4'
$unsupported = Join-Path $output 'unsupported-mpeg4-mp3.mp4'
$x264Options = 'threads=1:lookahead_threads=1:sliced_threads=0:sync_lookahead=0:deterministic=1'

Invoke-FixtureGenerator @(
    '-hide_banner', '-loglevel', 'error', '-y',
    '-f', 'lavfi', '-i', 'testsrc2=size=160x90:rate=10:duration=2',
    '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=44100:duration=2',
    '-map', '0:v:0', '-map', '1:a:0',
    '-c:v', 'libx264', '-preset', 'slow', '-crf', '18',
    '-pix_fmt', 'yuv420p', '-threads:v', '1', '-x264-params', $x264Options,
    '-c:a', 'aac', '-b:a', '96k', '-ac', '2', '-ar', '44100',
    '-map_metadata', '-1', '-fflags', '+bitexact',
    '-flags:v', '+bitexact', '-flags:a', '+bitexact',
    '-movflags', '+faststart+disable_chpl', '-write_tmcd', '0', $base
)

Invoke-FixtureGenerator @(
    '-hide_banner', '-loglevel', 'error', '-y',
    '-display_rotation:v:0', '90', '-i', $base,
    '-map', '0', '-c', 'copy', '-map_metadata', '-1',
    '-movflags', '+faststart+disable_chpl', $rotated
)

Invoke-FixtureGenerator @(
    '-hide_banner', '-loglevel', 'error', '-y',
    '-f', 'lavfi', '-i', 'testsrc2=size=160x90:rate=10:duration=1',
    '-f', 'lavfi', '-i', 'testsrc2=size=160x90:rate=5:duration=1',
    '-filter_complex', '[0:v][1:v]concat=n=2:v=1:a=0[v]',
    '-map', '[v]', '-fps_mode', 'vfr',
    '-c:v', 'libx264', '-preset', 'slow', '-crf', '18',
    '-pix_fmt', 'yuv420p', '-threads:v', '1', '-x264-params', $x264Options,
    '-map_metadata', '-1', '-fflags', '+bitexact', '-flags:v', '+bitexact',
    '-movflags', '+faststart+disable_chpl', '-write_tmcd', '0', $vfr
)

Invoke-FixtureGenerator @(
    '-hide_banner', '-loglevel', 'error', '-y',
    '-f', 'lavfi', '-i', 'testsrc2=size=160x90:rate=5:duration=1',
    '-f', 'lavfi', '-i', 'sine=frequency=330:sample_rate=48000:duration=1',
    '-map', '0:v:0', '-map', '1:a:0',
    '-c:v', 'mpeg4', '-q:v', '5', '-c:a', 'mp3', '-b:a', '64k',
    '-map_metadata', '-1', '-fflags', '+bitexact',
    '-flags:v', '+bitexact', '-flags:a', '+bitexact',
    '-movflags', '+faststart+disable_chpl', '-write_tmcd', '0', $unsupported
)

foreach ($entry in $expectedFiles.GetEnumerator()) {
    $path = Join-Path $output $entry.Key
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($actual -ne $entry.Value) {
        throw "Fixture hash mismatch for $($entry.Key). Expected $($entry.Value), found $actual."
    }
}

Write-Host "Generated and verified $($expectedFiles.Count) media fixtures in $output."
