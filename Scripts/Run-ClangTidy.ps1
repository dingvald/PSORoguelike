#Requires -Version 5.1
<#
.SYNOPSIS
    Runs clang-tidy over the project's C++ translation units using .clang-tidy.
.DESCRIPTION
    There is no compile_commands.json (the build is MSBuild via premake), so this
    script passes the compile flags explicitly using clang-cl driver mode, which
    auto-detects the installed MSVC toolchain and Windows SDK. Diagnostics are
    reported, not enforced; nothing gates the build on the result yet.
.EXAMPLE
    ./Scripts/Run-ClangTidy.ps1
.EXAMPLE
    ./Scripts/Run-ClangTidy.ps1 -Fix
#>
[CmdletBinding()]
param([switch]$Fix)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Find-ClangTool {
    param([Parameter(Mandatory)][string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $vsRoots = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio')
    ) | Where-Object { $_ -and (Test-Path $_) }
    foreach ($r in $vsRoots) {
        $hit = Get-ChildItem $r -Recurse -Filter "$Name.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\bin\\' } |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    throw "$Name.exe not found on PATH or under Visual Studio. Install LLVM, or the VS 'C++ Clang tools for Windows' component."
}

$clangTidy = Find-ClangTool 'clang-tidy'

$vcpkgInc = Join-Path $root 'vcpkg_installed\x64-windows\include'
if (-not (Test-Path $vcpkgInc)) {
    throw "vcpkg include tree not found at $vcpkgInc. Run Scripts\Setup-Windows.bat first."
}

# Compile flags mirror the premake projects (see Build.lua / Build-*.lua).
$compileFlags = @(
    '--driver-mode=cl',
    '/std:c++latest', '/EHsc', '/Zc:__cplusplus', '/Zc:preprocessor',
    "/I$(Join-Path $root 'Core\Source')",
    "/I$(Join-Path $root 'App\Source')",
    "/I$vcpkgInc",
    '/DRMLUI_SDL_VERSION_MAJOR=3', '/DWINDOWS', '/DDEBUG'
)

# clang-tidy runs on translation units (.cpp); headers are checked via inclusion
# and filtered by HeaderFilterRegex in .clang-tidy.
$dirs = @('Core\Source', 'App\Source') | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }
$cppFiles = @(foreach ($d in $dirs) {
        Get-ChildItem $d -Recurse -Include *.cpp -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -notmatch '[\\/]Backends[\\/]' }
    })

if ($cppFiles.Count -eq 0) { Write-Host 'No translation units found.'; return }

$tidyArgs = @('--quiet')
if ($Fix) { $tidyArgs += '--fix' }

# clang-tidy exits 0 for warning-level diagnostics (WarningsAsErrors is empty),
# so detect findings from the output text rather than the exit code. Capture via
# Start-Process with temp files: PowerShell 5.1 wraps native stderr as terminating
# errors when piped with 2>&1, which Start-Process sidesteps.
$failed = 0
foreach ($f in $cppFiles) {
    Write-Host "clang-tidy: $($f.FullName)" -ForegroundColor Cyan
    $tmpOut = [System.IO.Path]::GetTempFileName()
    $tmpErr = [System.IO.Path]::GetTempFileName()
    try {
        $procArgs = @($tidyArgs + $f.FullName + '--' + $compileFlags)
        Start-Process -FilePath $clangTidy -ArgumentList $procArgs -NoNewWindow -Wait `
            -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr | Out-Null
        $output = ((Get-Content $tmpOut -Raw) + (Get-Content $tmpErr -Raw))
    }
    finally {
        Remove-Item $tmpOut, $tmpErr -Force -ErrorAction SilentlyContinue
    }
    if ($output -and $output.Trim()) { Write-Host $output.TrimEnd() }
    if ($output -match ':\s*(warning|error):') { $failed++ }
}

if ($failed -gt 0) {
    Write-Host ("clang-tidy reported issues in {0} of {1} file(s)." -f $failed, $cppFiles.Count) -ForegroundColor Yellow
    exit 1
}
Write-Host ("clang-tidy clean across {0} file(s)." -f $cppFiles.Count) -ForegroundColor Green
