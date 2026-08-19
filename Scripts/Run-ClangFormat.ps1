#Requires -Version 5.1
<#
.SYNOPSIS
    Formats (or checks) the project's C++ sources with clang-format using .clang-format.
.DESCRIPTION
    Runs over Core/Source and App/Source, excluding vendored code under Backends.
    By default rewrites files in place. Pass -Check to only verify (non-zero exit
    if any file is mis-formatted) without modifying anything.
.EXAMPLE
    ./Scripts/Run-ClangFormat.ps1
.EXAMPLE
    ./Scripts/Run-ClangFormat.ps1 -Check
#>
[CmdletBinding()]
param([switch]$Check)

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

function Get-ProjectSources {
    $dirs = @('Core\Source', 'App\Source') | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }
    foreach ($d in $dirs) {
        Get-ChildItem $d -Recurse -Include *.h, *.cpp -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -notmatch '[\\/]Backends[\\/]' }
    }
}

$clangFormat = Find-ClangTool 'clang-format'
$files = @(Get-ProjectSources)
if ($files.Count -eq 0) { Write-Host 'No source files found.'; return }

if ($Check) {
    & $clangFormat --dry-run --Werror --style=file $files.FullName
    if ($LASTEXITCODE -ne 0) { throw 'Formatting issues found. Run Scripts\Run-ClangFormat.ps1 (no args) to fix.' }
    Write-Host ("All {0} file(s) correctly formatted." -f $files.Count)
}
else {
    & $clangFormat -i --style=file $files.FullName
    if ($LASTEXITCODE -ne 0) { throw 'clang-format failed.' }
    Write-Host ("Formatted {0} file(s)." -f $files.Count)
}
