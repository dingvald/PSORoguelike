# PSORoguelike

A turn-based roguelike inspired by Phantasy Star Online (GameCube), and its engine,
developed in parallel. Adapted from the sibling [UnnamedRoguelike](../UnnamedRoguelike)
project's engine architecture.

- **Language:** C++23
- **Windowing / rendering:** [SDL3](https://github.com/libsdl-org/SDL)
- **GUI:** [RmlUi](https://github.com/mikke89/RmlUi) (HTML/CSS-style UI), using the SDL native 2D renderer backend
- **Build system:** [premake5](https://premake.github.io/) generating a **Visual Studio 2026** solution
- **Dependencies:** [vcpkg](https://github.com/microsoft/vcpkg) manifest mode

Based on the Core/App layout of [TheCherno/ProjectTemplate](https://github.com/TheCherno/ProjectTemplate).

See [ARCHITECTURE.md](ARCHITECTURE.md) for the runtime design (Application lifecycle, layer
stack, events, and cross-layer messaging) and [docs/GDD.md](docs/GDD.md) for the game design
sketch.

## Project structure

```
PSORoguelike/
├─ Build.lua                 # premake workspace
├─ vcpkg.json               # dependency manifest (sdl3, sdl3-image, freetype, rmlui[freetype], entt, catch2)
├─ Core/                    # "Core" = the engine (static library)
│  ├─ Build-Core.lua
│  └─ Source/
│     ├─ Engine/            # engine code (Application, Layer, MessageBus, ECS Registry, Math, ...)
│     └─ Backends/          # vendored RmlUi SDL platform + renderer backend
├─ Core-Test/               # Catch2 unit tests for Core's pure-logic code
├─ App/                     # "App" = the game executable
│  ├─ Build-App.lua
│  ├─ Source/main.cpp
│  └─ Assets/               # fonts + .rml documents (copied next to the exe on build)
├─ Scripts/Setup-Windows.bat
├─ GenerateProjects-Windows.bat
├─ docs/GDD.md              # game design sketch
└─ Vendor/Binaries/Premake/Windows/premake5.exe   # premake built from master (has the vs2026 action)
```

`Core` and `App` are kept from the template: `Core` is the engine static library; `App` is
the roguelike executable that links it.

## Prerequisites

- **Visual Studio 2026** (v145 toolset, "Desktop development with C++" workload). The bundled
  vcpkg and CMake are used automatically.
- **git** on `PATH`.

## Building

From a normal command prompt:

```bat
Scripts\Setup-Windows.bat
```

This will, on first run:

1. Clone + bootstrap `vcpkg` into `Vendor/vcpkg/` (if missing).
2. `vcpkg install` the dependencies from `vcpkg.json` into `vcpkg_installed/x64-windows/`
   (SDL3, SDL3_image, FreeType, RmlUi, EnTT, Catch2 — built from source, cached afterwards).
3. Generate `PSORoguelike.slnx` via the bundled premake `vs2026` action.

Then open `PSORoguelike.slnx` in Visual Studio 2026 and build (Debug|x64), or from a
Developer prompt:

```bat
msbuild PSORoguelike.slnx /p:Configuration=Debug /p:Platform=x64
```

The build copies the required runtime DLLs and the `Assets/` folder next to `App.exe`
(`Binaries/windows-x86_64/<Config>/App/`). Running it opens a window rendering the
hello-world RmlUi document; press **Esc** to quit.

To only regenerate the solution (dependencies already installed):

```bat
GenerateProjects-Windows.bat
```

## Testing

```powershell
Scripts\Run-Tests.ps1 -Suite Core
```

## Notes

- **Why premake is vendored as a binary:** the `vs2026` action (new `.slnx` format, v145
  toolset) is only in premake `master`, not in any tagged release. `Vendor/Binaries/Premake/
  Windows/premake5.exe` was copied from the sibling UnnamedRoguelike project, which built it
  from source. To rebuild it yourself, clone
  [premake/premake-core](https://github.com/premake/premake-core) and run
  `nmake -f Bootstrap.mak windows` from a VS2026 Developer prompt, then copy
  `bin/release/premake5.exe` into `Vendor/Binaries/Premake/Windows/`.
- **RmlUi SDL backend:** `Core/Source/Backends/RmlUi_{Platform,Renderer}_SDL.{h,cpp}` are
  vendored from the RmlUi source (they ship as copy-into-your-project helpers, not part of the
  installed library). They are compiled with `RMLUI_SDL_VERSION_MAJOR=3`.
- **Configurations:** `Debug`, `Release`, `Dist` (all x64). Debug links the vcpkg debug libs,
  Release/Dist link the release libs.
- **PSO trademark note:** this is a private, non-commercial fan project inspired by Sega's
  Phantasy Star Online — it does not use or bundle any of that game's actual assets or code.
