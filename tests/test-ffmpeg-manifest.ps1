[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ManifestPath,
    [Parameter(Mandatory)][string]$FetchScript,
    [Parameter(Mandatory)][string]$BuildScript
)

$ErrorActionPreference = 'Stop'
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json

if ($manifest.version -cne '8.1.2' -or
    $manifest.sourceSha256 -cne '464BEB5E7BF0C311E68B45AE2F04E9CC2AF88851ABB4082231742A74D97B524C' -or
    $manifest.sourceSignatureSha256 -cne '0A0963FCCD70597838073F3E31B20F4A4D8CC2B5E577472C9A5A1F22624246F8' -or
    $manifest.signingKeyFingerprint -cne 'FCF986EA15E6E293A5644F10B4322F04D67658D8' -or
    $manifest.license -cne 'LGPL-2.1-or-later') {
    throw 'The pinned FFmpeg source identity or license changed.'
}
if ($manifest.staticSupport.packageVersion -cne '13.0.0.r505.g7d006b2ea-1' -or
    $manifest.staticSupport.sourceCommit -cne '7d006b2ea4b17da66e515f4494b86cc1adb52f24' -or
    $manifest.staticSupport.archiveSha256 -cne 'CD4673731E6A655E20505DABB59682985D8CF0F8091B503DD15A871645E9557D' -or
    $manifest.staticSupport.license -cne 'MIT AND BSD-3-Clause-Clear') {
    throw 'The pinned winpthreads support library identity or license changed.'
}
$licenseFiles = @($manifest.licenseFiles | ForEach-Object { "$($_.file):$($_.sha256)" } | Sort-Object)
$expectedLicenseFiles = @(
    'COPYING.LGPLv2.1:246041B6ECF9BC32D718A62C57877C78B5EB397B6467E74ED7AE2626AB189C30',
    'LICENSE.md:2E1D16C72FD74E12063776371DA757322F8B77589386532F4FD8634BDE7DE1AF'
) | Sort-Object
if (($licenseFiles -join "`n") -cne ($expectedLicenseFiles -join "`n")) {
    throw 'The pinned FFmpeg license file inventory changed.'
}

$requiredArguments = @(
    '--arch=x86',
    '--enable-shared',
    '--disable-static',
    '--disable-network',
    '--disable-everything',
    '--enable-decoder=h264,aac',
    '--enable-demuxer=mov',
    '--enable-parser=h264,aac',
    '--extra-libs=-Wl,-Bstatic,-lwinpthread,-Bdynamic'
)
foreach ($argument in $requiredArguments) {
    if ($argument -cnotin @($manifest.configureArguments)) {
        throw "Required FFmpeg configure argument is missing: $argument"
    }
}
if (@($manifest.configureArguments | Where-Object {
    $_ -match '^--enable-(gpl|nonfree|programs|network|protocol|encoder|muxer|filter|avdevice)'
}).Count -ne 0) {
    throw 'The FFmpeg manifest enables a feature outside the minimal media profile.'
}

$expectedDlls = @('avcodec-62.dll', 'avformat-62.dll', 'avutil-60.dll', 'swresample-6.dll', 'swscale-9.dll')
$expectedDlls = @($expectedDlls | Sort-Object)
$actualDlls = @($manifest.runtime | ForEach-Object { $_.file } | Sort-Object)
if (($expectedDlls -join "`n") -cne ($actualDlls -join "`n")) {
    throw 'The FFmpeg runtime manifest contains an unexpected DLL set.'
}
foreach ($runtime in $manifest.runtime) {
    if ($runtime.sha256 -notmatch '^[0-9A-F]{64}$' -or @($runtime.imports).Count -eq 0) {
        throw "$($runtime.file) lacks a pinned hash or import contract."
    }
}

$fetchText = Get-Content -LiteralPath $FetchScript -Raw
$buildText = Get-Content -LiteralPath $BuildScript -Raw
if ($fetchText -notmatch [regex]::Escape($manifest.sourceSha256) -or
    $fetchText -notmatch '--force-local' -or
    $buildText -notmatch 'SOURCE_DATE_EPOCH' -or
    $buildText -notmatch 'ffile-prefix-map' -or
    $buildText -notmatch 'audit-ffmpeg-runtime\.ps1') {
    throw 'The dependency scripts do not enforce the FFmpeg manifest contract.'
}

Write-Host 'FFmpeg manifest contract tests passed.'
