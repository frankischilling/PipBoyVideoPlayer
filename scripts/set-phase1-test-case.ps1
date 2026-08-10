[CmdletBinding(SupportsShouldProcess, DefaultParameterSetName = 'Apply')]
param(
    [Parameter(Mandatory)][string]$InstanceRoot,
    [Parameter(Mandatory)][ValidateSet(
        'PBVP Phase 1 Base',
        'PBVP Phase 1 VUI Plus',
        'PBVP Phase 1 Extended',
        'PBVP Phase 1 Extended No Pip-Boy Tweaks')][string]$ProfileName,
    [string]$RtssProfilePath,
    [Parameter(Mandatory, ParameterSetName = 'Apply')][int]$Width,
    [Parameter(Mandatory, ParameterSetName = 'Apply')][int]$Height,
    [Parameter(Mandatory, ParameterSetName = 'Apply')]
    [ValidateSet('Fullscreen', 'Windowed')][string]$DisplayMode,
    [Parameter(ParameterSetName = 'Apply')]
    [ValidateSet(30, 60, 90, 120)][int]$FpsCap = 60,
    [Parameter(Mandatory, ParameterSetName = 'Apply')]
    [ValidateSet('On', 'Off')][string]$VSync,
    [Parameter(Mandatory, ParameterSetName = 'Restore')][switch]$Restore,
    [switch]$SkipFrameCap
)

$ErrorActionPreference = 'Stop'
$utf8NoBom = [Text.UTF8Encoding]::new($false)
$allowedResolutions = @(
    '1280x720',
    '1920x1080',
    '2560x1440',
    '3440x1440',
    '1280x960'
)

function Resolve-ContainedPath {
    param(
        [Parameter(Mandatory)][string]$Parent,
        [Parameter(Mandatory)][string]$Child
    )

    $parentPath = [IO.Path]::GetFullPath($Parent).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar)
    $childPath = [IO.Path]::GetFullPath((Join-Path $parentPath $Child))
    if (-not $childPath.StartsWith(
            $parentPath + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escaped its expected parent: $Child"
    }
    return $childPath
}

function Set-IniValue {
    param(
        [Parameter(Mandatory)][string]$Content,
        [Parameter(Mandatory)][string]$Key,
        [Parameter(Mandatory)][string]$Value,
        [Parameter(Mandatory)][string]$FileName
    )

    $escapedKey = [Regex]::Escape($Key)
    $pattern = "(?m)^(?<prefix>\s*$escapedKey\s*=\s*)[^;\r\n]*(?<suffix>\s*(?:;[^\r\n]*)?)$"
    $matches = [Regex]::Matches($Content, $pattern)
    if ($matches.Count -ne 1) {
        throw "Expected exactly one '$Key' setting in $FileName; found $($matches.Count)."
    }
    $regex = [Regex]::new($pattern)
    return $regex.Replace($Content, '${prefix}' + $Value + '${suffix}', 1)
}

function Write-TextAtomically {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Content
    )

    $temporary = "$Path.pbvp-$([Guid]::NewGuid().ToString('N')).tmp"
    try {
        [IO.File]::WriteAllText($temporary, $Content, $utf8NoBom)
        Move-Item -LiteralPath $temporary -Destination $Path -Force
    } finally {
        if (Test-Path -LiteralPath $temporary) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

function Restore-Bytes {
    param([Parameter(Mandatory)][hashtable]$Snapshots)

    foreach ($entry in $Snapshots.GetEnumerator()) {
        [IO.File]::WriteAllBytes([string]$entry.Key, [byte[]]$entry.Value)
    }
}

function Assert-FileWritable {
    param([Parameter(Mandatory)][string]$Path)

    try {
        $stream = [IO.File]::Open(
            $Path,
            [IO.FileMode]::Open,
            [IO.FileAccess]::Write,
            [IO.FileShare]::ReadWrite)
        $stream.Dispose()
    } catch {
        throw "The test-case file is not writable: $Path"
    }
}

$instance = (Resolve-Path -LiteralPath $InstanceRoot).Path
foreach ($process in Get-Process -Name 'ModOrganizer' -ErrorAction SilentlyContinue) {
    try {
        if (-not [string]::IsNullOrWhiteSpace($process.Path) -and
            [IO.Path]::GetFullPath((Split-Path -Parent $process.Path)).TrimEnd(
                [IO.Path]::DirectorySeparatorChar) -ieq $instance.TrimEnd(
                [IO.Path]::DirectorySeparatorChar)) {
            throw 'Close Mod Organizer before applying or restoring a Phase 1 test case.'
        }
    } catch {
        if ($_.Exception.Message -eq
            'Close Mod Organizer before applying or restoring a Phase 1 test case.') {
            throw
        }
    }
}
$profilesRoot = Resolve-ContainedPath -Parent $instance -Child 'profiles'
$profile = Resolve-ContainedPath -Parent $profilesRoot -Child $ProfileName
if (-not (Test-Path -LiteralPath $profile -PathType Container)) {
    throw "The isolated MO2 profile is missing: $ProfileName"
}
$saves = Join-Path $profile 'saves'
if ((Test-Path -LiteralPath $saves -PathType Container) -and
    @(Get-ChildItem -LiteralPath $saves -Force).Count -ne 0) {
    throw "The isolated test profile contains save data: $ProfileName"
}

$modList = Join-Path $profile 'modlist.txt'
if (-not (Test-Path -LiteralPath $modList -PathType Leaf)) {
    throw 'The isolated test profile has no mod list.'
}
$enabledMods = @(Get-Content -LiteralPath $modList)
if (@($enabledMods | Where-Object {
        $_ -eq '+Pip-Boy Video Player - Dev'
    }).Count -ne 1) {
    throw 'The development mod is not enabled exactly once in the selected profile.'
}
if (@($enabledMods | Where-Object {
        $_ -eq '+Pip-Boy Video Player - Phase 1 Save Guard'
    }).Count -ne 1) {
    throw 'The Phase 1 save guard is not enabled exactly once in the selected profile.'
}

$rtssProfile = $null
if ($SkipFrameCap) {
    if (-not [string]::IsNullOrWhiteSpace($RtssProfilePath)) {
        throw 'Do not provide an RTSS profile when -SkipFrameCap is present.'
    }
} else {
    if ([string]::IsNullOrWhiteSpace($RtssProfilePath)) {
        throw 'Provide an RTSS profile or use -SkipFrameCap for a display-only case.'
    }
    $rtssProfile = [IO.Path]::GetFullPath($RtssProfilePath)
    if ([IO.Path]::GetFileName($rtssProfile) -cne 'FalloutNV.exe.cfg') {
        throw 'The RTSS profile must be named FalloutNV.exe.cfg.'
    }
    if (-not (Test-Path -LiteralPath $rtssProfile -PathType Leaf)) {
        throw 'Create a FalloutNV.exe application profile in RTSS before configuring a test case.'
    }
}

$profileFiles = @(
    (Join-Path $profile 'fallout.ini'),
    (Join-Path $profile 'falloutprefs.ini'),
    (Join-Path $profile 'falloutcustom.ini')
)
foreach ($path in $profileFiles) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "The isolated profile is missing $([IO.Path]::GetFileName($path))."
    }
}

$allFiles = @($profileFiles)
if (-not $SkipFrameCap) {
    $allFiles += @($rtssProfile)
}
foreach ($path in $allFiles) {
    Assert-FileWritable -Path $path
}
$backupSuffix = '.pbvp-phase1.bak'
$statePath = if ($SkipFrameCap) {
    Join-Path $profile '.pbvp-phase1-display.state.json'
} else {
    "$rtssProfile.pbvp-phase1.state.json"
}

if ($Restore) {
    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
        throw 'No active PBVP Phase 1 test case was found.'
    }
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ($state.ProfileName -cne $ProfileName) {
        throw "The active test case belongs to '$($state.ProfileName)'. Restore that profile first."
    }
    foreach ($path in $allFiles) {
        $backup = "$path$backupSuffix"
        if (-not (Test-Path -LiteralPath $backup -PathType Leaf)) {
            throw "The test-case backup is missing: $backup"
        }
    }
    if ($PSCmdlet.ShouldProcess($ProfileName, 'Restore the Phase 1 display and RTSS baselines')) {
        foreach ($path in $allFiles) {
            $backup = "$path$backupSuffix"
            Move-Item -LiteralPath $backup -Destination $path -Force
        }
        Remove-Item -LiteralPath $statePath -Force
    }
    Write-Host "Restored the Phase 1 baseline for $ProfileName."
    return
}

$resolution = "${Width}x${Height}"
if ($resolution -notin $allowedResolutions) {
    throw "Unsupported Phase 1 test resolution: $resolution"
}

$existingState = $null
if (Test-Path -LiteralPath $statePath -PathType Leaf) {
    $existingState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ($existingState.ProfileName -cne $ProfileName) {
        throw "The active test case belongs to '$($existingState.ProfileName)'. Restore that profile first."
    }
} else {
    foreach ($path in $allFiles) {
        if (Test-Path -LiteralPath "$path$backupSuffix") {
            throw "A stale Phase 1 backup exists without its state file: $path$backupSuffix"
        }
    }
}

$fullScreenValue = if ($DisplayMode -eq 'Fullscreen') { '1' } else { '0' }
$presentInterval = if ($VSync -eq 'On') { '1' } else { '0' }
$newContent = @{}
foreach ($path in $profileFiles) {
    $name = [IO.Path]::GetFileName($path)
    $content = [IO.File]::ReadAllText($path)
    $content = Set-IniValue -Content $content -Key 'bFull Screen' `
        -Value $fullScreenValue -FileName $name
    $content = Set-IniValue -Content $content -Key 'iPresentInterval' `
        -Value $presentInterval -FileName $name
    if ($name -eq 'falloutprefs.ini') {
        $content = Set-IniValue -Content $content -Key 'iSize W' `
            -Value ([string]$Width) -FileName $name
        $content = Set-IniValue -Content $content -Key 'iSize H' `
            -Value ([string]$Height) -FileName $name
    }
    $newContent[$path] = $content
}

if (-not $SkipFrameCap) {
    $rtssContent = [IO.File]::ReadAllText($rtssProfile)
    $rtssContent = Set-IniValue -Content $rtssContent -Key 'Limit' `
        -Value ([string]$FpsCap) -FileName 'FalloutNV.exe.cfg'
    $rtssContent = Set-IniValue -Content $rtssContent -Key 'LimitDenominator' `
        -Value '1' -FileName 'FalloutNV.exe.cfg'
    $newContent[$rtssProfile] = $rtssContent
}

$snapshots = @{}
foreach ($path in $allFiles) {
    $snapshots[$path] = [IO.File]::ReadAllBytes($path)
}

$capDescription = if ($SkipFrameCap) { 'frame cap unchanged' } else { "$FpsCap FPS" }
if (-not $PSCmdlet.ShouldProcess(
        $ProfileName,
        "Set $resolution $DisplayMode, $capDescription, VSync $VSync")) {
    return
}

$createdBaseline = $false
try {
    if ($null -eq $existingState) {
        foreach ($path in $allFiles) {
            Copy-Item -LiteralPath $path -Destination "$path$backupSuffix"
        }
        $state = [ordered]@{
            ProfileName = $ProfileName
            CreatedUtc = [DateTime]::UtcNow.ToString('o')
        }
        [IO.File]::WriteAllText(
            $statePath,
            ($state | ConvertTo-Json) + [Environment]::NewLine,
            $utf8NoBom)
        $createdBaseline = $true
    }

    foreach ($path in $allFiles) {
        Write-TextAtomically -Path $path -Content $newContent[$path]
    }
} catch {
    Restore-Bytes -Snapshots $snapshots
    if ($createdBaseline) {
        foreach ($path in $allFiles) {
            Remove-Item -LiteralPath "$path$backupSuffix" -Force -ErrorAction SilentlyContinue
        }
        Remove-Item -LiteralPath $statePath -Force -ErrorAction SilentlyContinue
    }
    throw
}

Write-Host "Configured $ProfileName for $resolution $DisplayMode, $capDescription, VSync $VSync."
Write-Host 'Exit FalloutNV before selecting another case. Use -Restore when this profile is finished.'
