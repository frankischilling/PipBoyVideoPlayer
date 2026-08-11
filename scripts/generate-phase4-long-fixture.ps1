[CmdletBinding()]
param(
    [string]$FfmpegPath = 'ffmpeg',
    [string]$FfprobePath = 'ffprobe',
    [string]$OutputPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-host\phase4-fixtures\PBVP-Phase4-30Minute.mp4')
)

$ErrorActionPreference = 'Stop'
$expectedFfmpegHash = '74DB6C184A03DBA2BDFE23E1A1F41CF5A8385BC1DE6A7A1B26DB1DC541ABEF93'
$expectedFfprobeHash = '55BB6C6289367AE2383EFA86B26BF2596F8ADB72AC747360EB13DF162354161C'
$expectedFixtureHash = '431220B5D0F941E85E44671CDC46F04E43C3D6FA5AFA04988B35073E5C2FA239'

function Resolve-PinnedExecutable {
    param(
        [Parameter(Mandatory)][string]$CommandName,
        [Parameter(Mandatory)][string]$ExpectedHash
    )

    $command = Get-Command $CommandName -ErrorAction Stop
    if ($command.CommandType -ne 'Application') {
        throw "The command must resolve to an executable: $CommandName"
    }
    $item = Get-Item -LiteralPath $command.Source
    $path = if ($item.LinkType -eq 'SymbolicLink') { [string]$item.Target } else { $item.FullName }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $ExpectedHash) {
        throw "Pinned executable hash mismatch: $CommandName"
    }
    return $path
}

$ffmpeg = Resolve-PinnedExecutable -CommandName $FfmpegPath -ExpectedHash $expectedFfmpegHash
$ffprobe = Resolve-PinnedExecutable -CommandName $FfprobePath -ExpectedHash $expectedFfprobeHash
$output = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $output
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("pbvp-phase4-long-" + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
try {
    $segment = Join-Path $temporaryRoot 'segment.mp4'
    $x264Options = 'threads=1:lookahead_threads=1:sliced_threads=0:sync_lookahead=0:deterministic=1'
    & $ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-f', 'lavfi', '-i', 'color=c=0x101020:size=1280x720:rate=30:duration=10',
        '-filter:v', 'drawbox=x=mod(t*120\,1180):y=310:w=100:h=100:color=0x40ff80:t=fill',
        '-map', '0:v:0',
        '-c:v', 'libx264', '-preset', 'slow', '-crf', '23', '-g', '30',
        '-pix_fmt', 'yuv420p', '-threads:v', '1', '-x264-params', $x264Options,
        '-an', '-map_metadata', '-1', '-fflags', '+bitexact',
        '-flags:v', '+bitexact',
        '-movflags', '+faststart+disable_chpl', '-write_tmcd', '0', $segment)
    if ($LASTEXITCODE -ne 0) {
        throw 'The 10-second source segment could not be generated.'
    }

    & $ffmpeg @(
        '-hide_banner', '-loglevel', 'error', '-y',
        '-stream_loop', '179', '-i', $segment,
        '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=1800',
        '-t', '1800', '-map', '0:v:0', '-map', '1:a:0', '-c:v', 'copy',
        '-c:a', 'aac', '-b:a', '48k', '-ac', '2', '-ar', '48000',
        '-map_metadata', '-1', '-fflags', '+bitexact', '-flags:a', '+bitexact',
        '-movflags', '+faststart+disable_chpl',
        '-write_tmcd', '0', $output)
    if ($LASTEXITCODE -ne 0) {
        throw 'The 30-minute fixture could not be assembled.'
    }
} finally {
    $resolvedTemporary = [IO.Path]::GetFullPath($temporaryRoot)
    $systemTemporary = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTemporary.StartsWith($systemTemporary, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedTemporary)) {
        Remove-Item -LiteralPath $resolvedTemporary -Recurse -Force
    }
}

$probeJson = & $ffprobe @(
    '-v', 'error', '-show_entries',
    'format=duration:stream=index,codec_name,codec_type,width,height,r_frame_rate,sample_rate,channels',
    '-of', 'json', $output)
if ($LASTEXITCODE -ne 0) {
    throw 'The generated 30-minute fixture could not be probed.'
}
$probe = $probeJson | ConvertFrom-Json
$video = @($probe.streams | Where-Object codec_type -eq 'video')
$audio = @($probe.streams | Where-Object codec_type -eq 'audio')
$duration = [double]::Parse(
    [string]$probe.format.duration,
    [Globalization.CultureInfo]::InvariantCulture)
if ($video.Count -ne 1 -or $audio.Count -ne 1 -or
    $video[0].codec_name -cne 'h264' -or $video[0].width -ne 1280 -or
    $video[0].height -ne 720 -or $video[0].r_frame_rate -cne '30/1' -or
    $audio[0].codec_name -cne 'aac' -or $audio[0].sample_rate -cne '48000' -or
    $audio[0].channels -ne 2 -or [Math]::Abs($duration - 1800.0) -gt 0.05) {
    throw 'The generated fixture does not match the 30-minute 720p30 H.264 and AAC contract.'
}

$hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
if ($hash -cne $expectedFixtureHash) {
    throw "The generated long fixture hash changed. Expected $expectedFixtureHash, found $hash."
}
$item = Get-Item -LiteralPath $output
Write-Host "Phase 4 long fixture: $output"
Write-Host "SHA-256: $hash"
Write-Host "Bytes: $($item.Length)"
Write-Host "Duration seconds: $duration"
