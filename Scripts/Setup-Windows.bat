@echo off
setlocal
pushd "%~dp0.."

REM --- Restore/bootstrap vcpkg if missing ---
if not exist "Vendor\vcpkg\vcpkg.exe" (
  if not exist "Vendor\vcpkg\.git" (
    echo [Setup] Cloning vcpkg...
    git clone https://github.com/microsoft/vcpkg.git Vendor\vcpkg
  )
  echo [Setup] Bootstrapping vcpkg...
  call "Vendor\vcpkg\bootstrap-vcpkg.bat" -disableMetrics
)

REM --- Install dependencies (vcpkg manifest mode) ---
set "VCPKG_ROOT=%CD%\Vendor\vcpkg"
echo [Setup] Installing dependencies via vcpkg (this can take a while the first time)...
"Vendor\vcpkg\vcpkg.exe" install --triplet x64-windows
if errorlevel 1 goto :error

REM --- Generate the Visual Studio 2026 solution ---
echo [Setup] Generating Visual Studio 2026 solution...
"Vendor\Binaries\Premake\Windows\premake5.exe" --file=Build.lua vs2026
if errorlevel 1 goto :error

echo [Setup] Done.
popd
endlocal
pause
exit /b 0

:error
echo [Setup] FAILED. See output above.
popd
endlocal
pause
exit /b 1
