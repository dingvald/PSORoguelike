---
name: build-windows
description: Compile and build the full PSORoguelike Windows solution (Core, App, Editor, Core-Test, App-Test) via MSBuild. Use when the user asks to build, compile, or check that the project builds on Windows.
model: haiku
---

Build `PSORoguelike.slnx` via MSBuild. This compiles every project in the workspace: Core, App, Editor, and both test projects.

1. Locate MSBuild via `vswhere` (same approach as `Scripts/Run-Tests.ps1`'s `Find-MSBuild` — plain PATH/folder-name lookups can't reliably distinguish the VS2026 toolset the workspace targets from an older VS2022 also on the box):
   ```powershell
   $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
   $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
   ```
2. Build the solution from the repo root. Default to `Debug|x64` unless the user names a different configuration (`Debug`/`Release`/`Dist`):
   ```powershell
   & $msbuild "PSORoguelike.slnx" /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal /m
   ```
3. Check `$LASTEXITCODE`. On failure, pull the actual error lines (`error C`, `error MSB`, `error LNK`) out of the output rather than dumping the whole log back to the user.

If the build fails on a missing/unresolved source file (not a compile error in existing code), the premake-generated project files may be stale — a new source subfolder won't be picked up automatically (see the curated-build-globs behavior in `App-Test`/`Editor` premake files). Regenerate first via `GenerateProjects-Windows.bat` (runs `premake5 vs2026`), then rebuild. Don't regenerate speculatively — only when the failure mode points at it.
