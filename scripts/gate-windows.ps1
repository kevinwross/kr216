<#
.SYNOPSIS
    The kr216 gate: configure, build and test on this host.

.DESCRIPTION
    One shape across the machines, so the family gate can call any of them with
    the same arguments: -Preset selects the tree, -Krc and -Krld point at a
    toolchain when the rows that need one should run, and -SkipTests builds
    without running the suite.

    The row count is asserted. A suite that registers fewer rows still reports
    success, and a smaller suite is exactly the failure this catches.

.EXAMPLE
    ./scripts/gate-windows.ps1
    ./scripts/gate-windows.ps1 -Preset windows-msvc-debug -SkipTests
#>
[CmdletBinding()]
param(
    [string]$Preset = 'windows-msvc-debug',
    [string]$Krc = '',
    [string]$Krld = '',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# The row counts this gate expects, per preset. A preset that is not listed is
# configured and built but its count is only printed, because a number nobody
# measured is worse than no assertion.
$expected = @{
    'windows-msvc-debug' = 56
}

$doctor = Join-Path $root 'scripts/doctor.ps1'
if (Test-Path $doctor) {
    & pwsh -NoProfile -File $doctor -Repo kr216
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$arguments = @('--preset', $Preset)
if ($Krc) { $arguments += "-DKRG_KRC=$Krc" }
if ($Krld) { $arguments += "-DKRG_KRLD=$Krld" }

cmake @arguments
if ($LASTEXITCODE -ne 0) { throw "configure failed for $Preset" }
cmake --build --preset $Preset
if ($LASTEXITCODE -ne 0) { throw "build failed for $Preset" }

if ($expected.ContainsKey($Preset)) {
    $count = (ctest --preset $Preset -N 2>&1 | Select-String -Pattern 'Total Tests: (\d+)' | Select-Object -First 1)
    $rows = if ($count) { $count.Matches.Groups[1].Value } else { '0' }
    if ("$rows" -ne "$($expected[$Preset])") {
        throw "$Preset registered $rows rows, expected $($expected[$Preset]); a missing row reports success"
    }
    Write-Output "rows registered: $rows"
}

if (-not $SkipTests) {
    ctest --preset $Preset --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "tests failed for $Preset" }
}

Write-Output "GATE KR216: PASS ($Preset)"
