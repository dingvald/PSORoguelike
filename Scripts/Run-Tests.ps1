#Requires -Version 5.1
<#
.SYNOPSIS
    Builds and runs the Core-Test Catch2 suite.
.DESCRIPTION
    Builds the requested test project directly (not via the .slnx, to avoid
    any target-name escaping questions around the hyphen in the project name),
    then runs the resulting exe. Any remaining arguments are forwarded
    verbatim to the Catch2 executable, e.g. a tag filter or -s.

    App-Test doesn't exist yet (App has no pure-logic surface worth a Catch2
    harness yet) -- re-add an 'App' entry to $projects and the -Suite
    ValidateSet below once it does.
.EXAMPLE
    ./Scripts/Run-Tests.ps1
.EXAMPLE
    ./Scripts/Run-Tests.ps1 -Suite Core -Configuration Release
.EXAMPLE
    ./Scripts/Run-Tests.ps1 -Suite Core "[MessageBus]" -s
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Core', 'All')][string]$Suite = 'All',
    [ValidateSet('Debug', 'Release', 'Dist')][string]$Configuration = 'Debug',
    [Parameter(ValueFromRemainingArguments)][string[]]$CatchArgs
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Find-MSBuild {
    # The workspace targets the v145 (VS2026) toolset (see Build.lua). PATH
    # order or plain folder-name sorting can't reliably tell VS2026 ("18")
    # apart from an older VS2022 ("2022") also on the box, so prefer
    # vswhere (compares actual product versions) over both.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $hit = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null |
            Select-Object -First 1
        if ($hit) { return $hit }
    }
    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "MSBuild.exe not found via vswhere or on PATH. Install Visual Studio with the C++ workload."
}

$projects = @{
    Core = @{
        Proj = Join-Path $root 'Core-Test\Core-Test.vcxproj'
        Exe  = Join-Path $root "Binaries\windows-x86_64\$Configuration\Core-Test\Core-Test.exe"
    }
}

$suitesToRun = if ($Suite -eq 'All') { @('Core') } else { @($Suite) }
$msbuild = Find-MSBuild

$failed = 0
foreach ($name in $suitesToRun) {
    $info = $projects[$name]

    Write-Host "Building $name-Test ($Configuration|x64)..." -ForegroundColor Cyan
    & $msbuild $info.Proj "/p:Configuration=$Configuration" '/p:Platform=x64' '/nologo' '/v:minimal'
    if ($LASTEXITCODE -ne 0) {
        Write-Host "$name-Test build failed." -ForegroundColor Red
        $failed++
        continue
    }

    if (-not (Test-Path $info.Exe)) { throw "Expected test binary not found: $($info.Exe)" }

    Write-Host "Running $name-Test..." -ForegroundColor Cyan
    & $info.Exe @CatchArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "$name-Test reported failures." -ForegroundColor Yellow
        $failed++
    }
}

if ($failed -gt 0) {
    Write-Host ("{0} suite(s) failed." -f $failed) -ForegroundColor Yellow
    exit 1
}
Write-Host 'All test suites passed.' -ForegroundColor Green
