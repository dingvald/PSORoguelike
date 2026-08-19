@echo off
REM Regenerate the Visual Studio 2026 solution (dependencies must already be installed).
pushd "%~dp0"
"Vendor\Binaries\Premake\Windows\premake5.exe" --file=Build.lua vs2026
popd
pause
