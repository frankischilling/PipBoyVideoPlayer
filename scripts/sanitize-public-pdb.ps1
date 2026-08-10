[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputPdb,
    [Parameter(Mandatory)][string]$OutputPdb,
    [Parameter(Mandatory)][string]$BinaryPath,
    [string]$LlvmPdbUtil
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Read-UInt32 {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)][int]$Offset
    )

    if ($Offset -lt 0 -or $Offset + 4 -gt $Bytes.Length) {
        throw 'The PDB contains an out-of-range 32-bit field.'
    }
    [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Get-MsfLayout {
    param([Parameter(Mandatory)][byte[]]$Bytes)

    if ($Bytes.Length -lt 56) {
        throw 'The public PDB is too small to contain an MSF 7.0 header.'
    }
    $magic = [Text.Encoding]::ASCII.GetString($Bytes, 0, 26)
    if ($magic -ne "Microsoft C/C++ MSF 7.00`r`n") {
        throw 'The public PDB does not use the supported MSF 7.0 format.'
    }

    $blockSize = [int](Read-UInt32 -Bytes $Bytes -Offset 32)
    $freeBlockMapBlock = [int](Read-UInt32 -Bytes $Bytes -Offset 36)
    $numberOfBlocks = [int](Read-UInt32 -Bytes $Bytes -Offset 40)
    $directorySize = [int](Read-UInt32 -Bytes $Bytes -Offset 44)
    $blockMapAddress = [int](Read-UInt32 -Bytes $Bytes -Offset 52)
    if ($blockSize -lt 512 -or $blockSize -gt 65536 -or
        ($blockSize -band ($blockSize - 1)) -ne 0) {
        throw 'The public PDB has an unsupported MSF block size.'
    }
    if ($numberOfBlocks -le 0 -or
        [int64]$numberOfBlocks * $blockSize -gt $Bytes.Length) {
        throw 'The public PDB has an invalid MSF block count.'
    }
    if ($numberOfBlocks -gt $blockSize * 8) {
        throw 'The public PDB is too large for the supported single free-page-map interval.'
    }
    if ($freeBlockMapBlock -notin @(1, 2)) {
        throw 'The public PDB uses an unsupported free-page-map block.'
    }

    $directoryBlockCount = [int][Math]::Ceiling($directorySize / [double]$blockSize)
    if ($directoryBlockCount * 4 -gt $blockSize -or
        $blockMapAddress -lt 0 -or $blockMapAddress -ge $numberOfBlocks) {
        throw 'The public PDB has an unsupported MSF directory map.'
    }

    $directory = [byte[]]::new($directorySize)
    $directoryBlocks = [uint32[]]::new($directoryBlockCount)
    $remaining = $directorySize
    $destinationOffset = 0
    for ($index = 0; $index -lt $directoryBlockCount; $index++) {
        $mapOffset = $blockMapAddress * $blockSize + $index * 4
        $directoryBlock = [int](Read-UInt32 -Bytes $Bytes -Offset $mapOffset)
        if ($directoryBlock -lt 0 -or $directoryBlock -ge $numberOfBlocks) {
            throw 'The public PDB directory refers to an invalid block.'
        }
        $directoryBlocks[$index] = $directoryBlock
        $copyLength = [Math]::Min($remaining, $blockSize)
        [Buffer]::BlockCopy(
            $Bytes, $directoryBlock * $blockSize,
            $directory, $destinationOffset, $copyLength)
        $remaining -= $copyLength
        $destinationOffset += $copyLength
    }

    $streamCount = [int](Read-UInt32 -Bytes $directory -Offset 0)
    if ($streamCount -le 3 -or $streamCount -gt 65536) {
        throw 'The public PDB has an invalid stream count.'
    }
    $cursor = 4
    $streamSizes = [uint32[]]::new($streamCount)
    for ($index = 0; $index -lt $streamCount; $index++) {
        $streamSizes[$index] = Read-UInt32 -Bytes $directory -Offset $cursor
        $cursor += 4
    }

    $streamBlocks = [object[]]::new($streamCount)
    for ($streamIndex = 0; $streamIndex -lt $streamCount; $streamIndex++) {
        $streamSize = $streamSizes[$streamIndex]
        $blockCount = if ($streamSize -eq [uint32]::MaxValue) {
            0
        } else {
            [int][Math]::Ceiling($streamSize / [double]$blockSize)
        }
        $blocks = [uint32[]]::new($blockCount)
        for ($blockIndex = 0; $blockIndex -lt $blockCount; $blockIndex++) {
            $block = Read-UInt32 -Bytes $directory -Offset $cursor
            $cursor += 4
            if ($block -ge $numberOfBlocks) {
                throw 'A PDB stream refers to an invalid MSF block.'
            }
            $blocks[$blockIndex] = $block
        }
        $streamBlocks[$streamIndex] = $blocks
    }
    if ($cursor -gt $directory.Length) {
        throw 'The PDB stream directory is truncated.'
    }

    [pscustomobject]@{
        BlockSize = $blockSize
        NumberOfBlocks = $numberOfBlocks
        BlockMapAddress = $blockMapAddress
        DirectoryBlocks = $directoryBlocks
        StreamSizes = $streamSizes
        StreamBlocks = $streamBlocks
    }
}

function Get-MsfStream {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)]$Layout,
        [Parameter(Mandatory)][int]$StreamIndex
    )

    if ($StreamIndex -lt 0 -or $StreamIndex -ge $Layout.StreamSizes.Length) {
        throw 'The requested PDB stream does not exist.'
    }
    $sizeValue = $Layout.StreamSizes[$StreamIndex]
    if ($sizeValue -eq [uint32]::MaxValue) {
        throw 'The requested PDB stream is not present.'
    }
    $size = [int]$sizeValue
    $result = [byte[]]::new($size)
    $remaining = $size
    $destinationOffset = 0
    foreach ($blockValue in @($Layout.StreamBlocks[$StreamIndex])) {
        $copyLength = [Math]::Min($remaining, $Layout.BlockSize)
        [Buffer]::BlockCopy(
            $Bytes, [int]$blockValue * $Layout.BlockSize,
            $result, $destinationOffset, $copyLength)
        $remaining -= $copyLength
        $destinationOffset += $copyLength
    }
    if ($remaining -ne 0) {
        throw 'The requested PDB stream is truncated.'
    }
    ,$result
}

function Set-MsfStream {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)]$Layout,
        [Parameter(Mandatory)][int]$StreamIndex,
        [Parameter(Mandatory)][byte[]]$StreamBytes
    )

    if ($Layout.StreamSizes[$StreamIndex] -ne $StreamBytes.Length) {
        throw 'The cleaned DBI stream changed size.'
    }
    $remaining = $StreamBytes.Length
    $sourceOffset = 0
    foreach ($blockValue in @($Layout.StreamBlocks[$StreamIndex])) {
        $copyLength = [Math]::Min($remaining, $Layout.BlockSize)
        [Buffer]::BlockCopy(
            $StreamBytes, $sourceOffset,
            $Bytes, [int]$blockValue * $Layout.BlockSize, $copyLength)
        $remaining -= $copyLength
        $sourceOffset += $copyLength
    }
    if ($remaining -ne 0) {
        throw 'The cleaned DBI stream could not be written completely.'
    }
}

function New-PathNeutralValue {
    param([Parameter(Mandatory)][string]$Path)

    $leaf = ($Path -split '[\\/]')[-1]
    $prefix = 'pbvp-public\'
    $paddingLength = $Path.Length - $prefix.Length - $leaf.Length
    if ($paddingLength -lt 1) {
        $prefix = 'pbvp\'
        $paddingLength = $Path.Length - $prefix.Length - $leaf.Length
    }
    if ($paddingLength -lt 1) {
        return 'p' + ('_' * ($Path.Length - 1))
    }
    $replacement = $prefix + ('_' * $paddingLength) + $leaf
    if ($replacement.Length -ne $Path.Length) {
        throw 'A path-neutral PDB name changed the DBI record length.'
    }
    $replacement
}

function Clear-UnreferencedMsfBlocks {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)]$Layout
    )

    $referenced = [Collections.Generic.HashSet[int]]::new()
    foreach ($block in @(0, 1, 2, $Layout.BlockMapAddress)) {
        if ($block -ge 0 -and $block -lt $Layout.NumberOfBlocks) {
            [void]$referenced.Add([int]$block)
        }
    }
    foreach ($block in @($Layout.DirectoryBlocks)) {
        [void]$referenced.Add([int]$block)
    }
    foreach ($blocks in $Layout.StreamBlocks) {
        foreach ($block in @($blocks)) {
            [void]$referenced.Add([int]$block)
        }
    }

    $zeroBlock = [byte[]]::new($Layout.BlockSize)
    $cleared = 0
    for ($block = 0; $block -lt $Layout.NumberOfBlocks; $block++) {
        if ($referenced.Contains($block)) { continue }
        [Buffer]::BlockCopy(
            $zeroBlock, 0,
            $Bytes, $block * $Layout.BlockSize, $Layout.BlockSize)
        $cleared++
    }
    $cleared
}

function Clear-MsfSlackBytes {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)]$Layout
    )

    $cleared = 0
    for ($streamIndex = 0; $streamIndex -lt $Layout.StreamSizes.Length; $streamIndex++) {
        $streamSize = $Layout.StreamSizes[$streamIndex]
        if ($streamSize -in @(0, [uint32]::MaxValue)) { continue }
        $usedInLastBlock = [int]($streamSize % $Layout.BlockSize)
        if ($usedInLastBlock -eq 0) { continue }
        $blocks = @($Layout.StreamBlocks[$streamIndex])
        $lastBlock = [int]$blocks[$blocks.Count - 1]
        $start = $lastBlock * $Layout.BlockSize + $usedInLastBlock
        $length = $Layout.BlockSize - $usedInLastBlock
        [Array]::Clear($Bytes, $start, $length)
        $cleared += $length
    }

    $directorySize = 4 + $Layout.StreamSizes.Length * 4
    foreach ($blocks in $Layout.StreamBlocks) {
        $directorySize += @($blocks).Count * 4
    }
    $directoryRemainder = $directorySize % $Layout.BlockSize
    if ($directoryRemainder -ne 0) {
        $lastDirectoryBlock = [int]$Layout.DirectoryBlocks[$Layout.DirectoryBlocks.Length - 1]
        $start = $lastDirectoryBlock * $Layout.BlockSize + $directoryRemainder
        $length = $Layout.BlockSize - $directoryRemainder
        [Array]::Clear($Bytes, $start, $length)
        $cleared += $length
    }

    $directoryBlockMapBytes = $Layout.DirectoryBlocks.Length * 4
    if ($directoryBlockMapBytes -lt $Layout.BlockSize) {
        $start = $Layout.BlockMapAddress * $Layout.BlockSize + $directoryBlockMapBytes
        $length = $Layout.BlockSize - $directoryBlockMapBytes
        [Array]::Clear($Bytes, $start, $length)
        $cleared += $length
    }
    $cleared
}

function Assert-NoAbsolutePath {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)][string]$Label
    )

    $patterns = @(
        '(?i)[A-Z]:[\\/]',
        '(?i)\\\\[A-Z0-9._$ -]+[\\/][A-Z0-9._$ -]+[\\/]'
    )
    $views = @(
        [pscustomobject]@{ Name = 'single-byte'; Text = [Text.Encoding]::GetEncoding('ISO-8859-1').GetString($Bytes) },
        [pscustomobject]@{ Name = 'UTF-16LE'; Text = [Text.Encoding]::Unicode.GetString($Bytes) },
        [pscustomobject]@{ Name = 'UTF-16BE'; Text = [Text.Encoding]::BigEndianUnicode.GetString($Bytes) }
    )
    foreach ($view in $views) {
        foreach ($pattern in $patterns) {
            $pathMatch = [regex]::Match($view.Text, $pattern)
            if ($pathMatch.Success) {
                throw "$Label still contains an absolute path in its $($view.Name) view at offset $($pathMatch.Index)."
            }
        }
    }
}

function Invoke-LlvmText {
    param(
        [Parameter(Mandatory)][string]$Tool,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    $lines = @(& $Tool @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "LLVM validation failed: $($Arguments -join ' ')"
    }
    $lines
}

$input = (Resolve-Path -LiteralPath $InputPdb).Path
$binary = (Resolve-Path -LiteralPath $BinaryPath).Path
$output = [IO.Path]::GetFullPath($OutputPdb)
if ($input -eq $output) {
    throw 'Input and output PDB paths must be different.'
}
if (Test-Path -LiteralPath $output) {
    throw "Refusing to replace an existing output PDB: $output"
}
$outputDirectory = Split-Path -Parent $output
if (-not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
    throw "The output directory does not exist: $outputDirectory"
}

$toolPath = if ($LlvmPdbUtil) {
    (Resolve-Path -LiteralPath $LlvmPdbUtil).Path
} else {
    (Get-Command llvm-pdbutil -ErrorAction Stop).Source
}
$versionText = (Invoke-LlvmText -Tool $toolPath -Arguments @('--version')) -join "`n"
if ($versionText -notmatch '(?m)^\s*LLVM version 22\.1\.0\s*$') {
    throw 'Public symbol validation requires llvm-pdbutil 22.1.0.'
}
$llvmReadObj = Join-Path (Split-Path -Parent $toolPath) 'llvm-readobj.exe'
if (-not (Test-Path -LiteralPath $llvmReadObj -PathType Leaf)) {
    throw 'The matching llvm-readobj executable is missing.'
}

$sourceBytes = [IO.File]::ReadAllBytes($input)
$layout = Get-MsfLayout -Bytes $sourceBytes
$dbiStream = Get-MsfStream -Bytes $sourceBytes -Layout $layout -StreamIndex 3
$latin1 = [Text.Encoding]::GetEncoding('ISO-8859-1')
$dbiText = $latin1.GetString($dbiStream)
$absolutePathPattern = [regex]::new(
    '(?i)(?:[A-Z]:[\\/]|\\\\[^\\/\x00]+[\\/][^\\/\x00]+[\\/])[^\x00]*?(?=\x00)',
    [Text.RegularExpressions.RegexOptions]::CultureInvariant)
$matches = @($absolutePathPattern.Matches($dbiText))
if ($matches.Count -lt 1) {
    throw 'The raw stripped PDB contains no DBI paths to clean.'
}

$characters = $dbiText.ToCharArray()
foreach ($match in $matches) {
    $replacement = New-PathNeutralValue -Path $match.Value
    for ($index = 0; $index -lt $replacement.Length; $index++) {
        $characters[$match.Index + $index] = $replacement[$index]
    }
}
$cleanDbiBytes = $latin1.GetBytes((-join $characters))
if ($cleanDbiBytes.Length -ne $dbiStream.Length) {
    throw 'The cleaned DBI stream changed byte length.'
}

$outputBytes = [byte[]]::new($sourceBytes.Length)
[Buffer]::BlockCopy($sourceBytes, 0, $outputBytes, 0, $sourceBytes.Length)
Set-MsfStream -Bytes $outputBytes -Layout $layout -StreamIndex 3 -StreamBytes $cleanDbiBytes

$outputLayout = Get-MsfLayout -Bytes $outputBytes
for ($streamIndex = 0; $streamIndex -lt $outputLayout.StreamSizes.Length; $streamIndex++) {
    if ($outputLayout.StreamSizes[$streamIndex] -in @(0, [uint32]::MaxValue)) { continue }
    $stream = Get-MsfStream -Bytes $outputBytes -Layout $outputLayout -StreamIndex $streamIndex
    Assert-NoAbsolutePath -Bytes $stream -Label "PDB stream $streamIndex"
}
$clearedBlockCount = Clear-UnreferencedMsfBlocks -Bytes $outputBytes -Layout $outputLayout
$clearedSlackByteCount = Clear-MsfSlackBytes -Bytes $outputBytes -Layout $outputLayout
Assert-NoAbsolutePath -Bytes $outputBytes -Label 'The cleaned PDB file'

$createdOutput = $false
try {
    [IO.File]::WriteAllBytes($output, $outputBytes)
    $createdOutput = $true

    foreach ($option in @('-summary', '-publics', '-fpo', '-section-contribs')) {
        $sourceDump = (Invoke-LlvmText -Tool $toolPath -Arguments @('dump', $option, $input)) -join "`n"
        $outputDump = (Invoke-LlvmText -Tool $toolPath -Arguments @('dump', $option, $output)) -join "`n"
        if ($sourceDump -cne $outputDump) {
            throw "The cleaned PDB changed LLVM $option output."
        }
    }
    $null = @(Invoke-LlvmText -Tool $toolPath -Arguments @('diadump', '--compilands', $output))

    $pdbSummary = (Invoke-LlvmText -Tool $toolPath -Arguments @('dump', '-summary', $output)) -join "`n"
    $imageSummary = (Invoke-LlvmText -Tool $llvmReadObj -Arguments @('--coff-debug-directory', $binary)) -join "`n"
    $pdbGuid = [regex]::Match($pdbSummary, '(?m)^\s*GUID:\s*(\{[^}]+\})\s*$').Groups[1].Value
    $pdbAge = [regex]::Match($pdbSummary, '(?m)^\s*Age:\s*(\d+)\s*$').Groups[1].Value
    $imageGuid = [regex]::Match($imageSummary, '(?m)^\s*PDBGUID:\s*(\{[^}]+\})\s*$').Groups[1].Value
    $imageAge = [regex]::Match($imageSummary, '(?m)^\s*PDBAge:\s*(\d+)\s*$').Groups[1].Value
    $imagePdbName = [regex]::Match($imageSummary, '(?m)^\s*PDBFileName:\s*(.+?)\s*$').Groups[1].Value
    if (-not $pdbGuid -or -not $pdbAge -or -not $imageGuid -or -not $imageAge -or -not $imagePdbName) {
        throw 'Could not read the PDB identity from the symbols and plugin image.'
    }
    if ($pdbGuid -cne $imageGuid -or $pdbAge -cne $imageAge) {
        throw 'The cleaned PDB does not match the plugin image.'
    }
    if ([IO.Path]::GetFileName($output) -cne $imagePdbName) {
        throw 'The cleaned PDB filename does not match the plugin CodeView record.'
    }
} catch {
    if ($createdOutput -and (Test-Path -LiteralPath $output)) {
        Remove-Item -LiteralPath $output -Force
    }
    throw
}

Write-Host "Prepared matching public symbols after cleaning $($matches.Count) absolute DBI path fields, $clearedBlockCount unused MSF blocks, and $clearedSlackByteCount slack bytes."
