[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SanitizerPath,
    [Parameter(Mandatory)][string]$InputPdb,
    [Parameter(Mandatory)][string]$BinaryPath,
    [Parameter(Mandatory)][string]$LlvmPdbUtil
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = [IO.Path]::Combine(
    [IO.Path]::GetTempPath(),
    "pbvp-pdb-test-$([Guid]::NewGuid().ToString('N'))")
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null

try {
    $inputHash = (Get-FileHash -LiteralPath $InputPdb -Algorithm SHA256).Hash
    $inputLength = (Get-Item -LiteralPath $InputPdb).Length
    $outputPdb = Join-Path $temporaryRoot 'PipBoyVideoPlayer.pdb'

    & $SanitizerPath `
        -InputPdb $InputPdb `
        -OutputPdb $outputPdb `
        -BinaryPath $BinaryPath `
        -LlvmPdbUtil $LlvmPdbUtil
    if (-not (Test-Path -LiteralPath $outputPdb -PathType Leaf)) {
        throw 'The sanitizer did not create the public PDB.'
    }
    if ((Get-Item -LiteralPath $outputPdb).Length -ne $inputLength) {
        throw 'The sanitizer changed the PDB byte length.'
    }
    if ((Get-FileHash -LiteralPath $InputPdb -Algorithm SHA256).Hash -cne $inputHash) {
        throw 'The sanitizer changed the linker-generated PDB.'
    }
    if ((Get-FileHash -LiteralPath $outputPdb -Algorithm SHA256).Hash -ceq $inputHash) {
        throw 'The sanitizer did not change the public PDB.'
    }

    $refusedSamePath = $false
    try {
        & $SanitizerPath `
            -InputPdb $InputPdb `
            -OutputPdb $InputPdb `
            -BinaryPath $BinaryPath `
            -LlvmPdbUtil $LlvmPdbUtil
    } catch {
        if ($_.Exception.Message -notmatch 'must be different') {
            throw
        }
        $refusedSamePath = $true
    }
    if (-not $refusedSamePath) {
        throw 'The sanitizer accepted the source PDB as its output path.'
    }

    Write-Host 'Public PDB sanitizer tests passed.'
} finally {
    $resolvedTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
    $systemTemporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTemporaryRoot.StartsWith(
            $systemTemporaryRoot,
            [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolvedTemporaryRoot) -like 'pbvp-pdb-test-*' -and
        (Test-Path -LiteralPath $resolvedTemporaryRoot)) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
