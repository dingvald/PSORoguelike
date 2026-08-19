-- Absolute, native-separator paths to the vcpkg DLL output folders, resolved at
-- generation time (premake runs from the workspace root).
-- Relative paths here are resolved against this script's directory (App/), so
-- reach up to the workspace root with "../".
local VcpkgBinRelease = path.translate(path.getabsolute("../vcpkg_installed/x64-windows/bin"), "\\")
local VcpkgBinDebug   = path.translate(path.getabsolute("../vcpkg_installed/x64-windows/debug/bin"), "\\")

project "App"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++23"
   staticruntime "off"

   files { "Source/**.h", "Source/**.cpp", "Assets/**" }

   includedirs
   {
      "Source",

      -- Include Core (the engine)
      "../Core/Source",

      -- vcpkg dependencies
      ("../" .. VcpkgDir .. "/include"),
   }

   defines { "RMLUI_SDL_VERSION_MAJOR=3" }

   -- RegisterComponents.cpp instantiates entt::meta registration templates for
   -- every component in one translation unit; the growing component count
   -- (BodyPartHealthComponent/EvasionComponent/WeaponComponent among the
   -- newest) pushed it past the default COFF section-count limit (C1128).
   filter "toolset:msc*"
       buildoptions { "/bigobj" }
   filter {}

   -- Libraries common to all configurations (same name in debug/release vcpkg trees).
   links
   {
      "Core",
      "SDL3",
      "SDL3_image",
      "rmlui",
      "rmlui_debugger",
   }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   -- Run from the output dir so the copied DLLs and assets are found.
   debugdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { "WINDOWS" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"
       libdirs { ("../" .. VcpkgDir .. "/debug/lib") }
       links { "freetyped" }
       postbuildcommands { 'xcopy /Y /D /I /Q "' .. VcpkgBinDebug .. '\\*.dll" "%{cfg.targetdir}"' }

   filter "configurations:Release or configurations:Dist"
       runtime "Release"
       optimize "On"
       libdirs { ("../" .. VcpkgDir .. "/lib") }
       links { "freetype" }
       postbuildcommands { 'xcopy /Y /D /I /Q "' .. VcpkgBinRelease .. '\\*.dll" "%{cfg.targetdir}"' }

   filter "configurations:Release"
       defines { "RELEASE" }
       symbols "On"

   filter "configurations:Dist"
       defines { "DIST" }
       symbols "Off"

   -- Copy the runtime assets (fonts, .rml documents) next to the executable.
   filter {}
       postbuildcommands { 'xcopy /y /d /s /i /q /e "%{prj.location}Assets" "%{cfg.targetdir}\\Assets\\"' }

   -- Assets are "None" items, which Visual Studio's Fast Up-to-Date Check ignores when
   -- deciding whether to invoke MSBuild at all. Without this, editing only an asset (e.g.
   -- slot_picker.rml) with no source changes makes VS skip the build entirely, so the postbuild
   -- copy above never runs and the binary directory serves stale assets.
   filter {}
       fastuptodate "Off"

   -- .glsl shader sources are compiled offline to SPIR-V (.spv) via
   -- Scripts/Compile-Shaders.ps1 (glslangValidator), not by MSBuild --
   -- force them (and the compiled .spv output) to plain "None" items so
   -- Visual Studio doesn't try to auto-assign a compiler item type by
   -- extension.
   filter { "files:**.glsl or **.spv" }
       buildaction "None"
