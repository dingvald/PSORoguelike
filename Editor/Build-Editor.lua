-- Absolute, native-separator paths to the vcpkg DLL output folders, resolved at
-- generation time (premake runs from the workspace root).
-- Relative paths here are resolved against this script's directory (Editor/), so
-- reach up to the workspace root with "../".
local VcpkgBinRelease = path.translate(path.getabsolute("../vcpkg_installed/x64-windows/bin"), "\\")
local VcpkgBinDebug   = path.translate(path.getabsolute("../vcpkg_installed/x64-windows/debug/bin"), "\\")

-- The editor reuses App's runtime assets (fonts, for now -- content Data
-- directories join this once a sub-editor needs to load/save them) so it
-- doesn't need its own copy checked in. Its own Assets/RML documents are
-- overlaid on top -- see the postbuild commands below.
local AppAssets = path.translate(path.getabsolute("../App/Assets"), "\\")
-- Forward-slash absolute path (safe inside a C string literal) so content
-- editors write Data/ JSON back to the game's SOURCE assets, not the
-- postbuild-copied runtime folder under Binaries/ (which is gitignored and
-- gets re-copied from source on every build).
local AppAssetsFwd = path.getabsolute("../App/Assets")
local EditorRml = path.translate(path.getabsolute("Assets/RML"), "\\")

project "Editor"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++23"
   staticruntime "off"

   files
   {
      "Source/**.h", "Source/**.cpp", "Assets/**",

      -- App is a ConsoleApp (not a library) so it can't be linked; compile its
      -- component definitions directly instead -- content editors need
      -- RegisterComponents/RenderableComponent to enumerate and preview real
      -- entity prefabs, mirrors UnnamedRoguelike's Editor/Build-Editor.lua.
      "../App/Source/Components/**.h",
      "../App/Source/Components/**.cpp",
      -- EquipmentComponent::AttachHandlers calls ComputeEffectiveStats to
      -- contribute a Before<Action>Event's attacker_stats -- pull in
      -- Combat/'s definitions too, or that call is an undefined symbol at
      -- link time (Editor doesn't link against App, which is a ConsoleApp).
      "../App/Source/Combat/**.h",
      "../App/Source/Combat/**.cpp",
      -- AffixEditorLayer/PrefabEditorLayer read/write Affix content directly.
      "../App/Source/Items/**.h",
      "../App/Source/Items/**.cpp",
      -- RegistryRenderableLookup resolves a live entity's RenderableComponent
      -- into the RenderableTile content editors draw for palette icons/canvas
      -- previews -- reused as-is rather than duplicating it Editor-side.
      "../App/Source/Render/**.h",
      "../App/Source/Render/**.cpp",
   }

   includedirs
   {
      "Source",

      -- Include Core (the engine)
      "../Core/Source",

      -- So "Components/..." above resolves the same way it does for App.
      "../App/Source",

      -- vcpkg dependencies
      ("../" .. VcpkgDir .. "/include"),
   }

   defines { "RMLUI_SDL_VERSION_MAJOR=3", 'PSR_APP_ASSETS_DIR="' .. AppAssetsFwd .. '"' }

   -- RegisterComponents.cpp instantiates entt::meta registration templates for
   -- every component in one translation unit -- same COFF section-count fix
   -- (C1128) as App/Build-App.lua, now that Editor compiles it too.
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

   -- Two-stage asset copy: bring in App's runtime assets (fonts) first, then
   -- overlay the editor's own RmlUi chrome documents into the same Assets/RML
   -- folder -- mirrors UnnamedRoguelike's Editor/Build-Editor.lua pattern.
   filter {}
       postbuildcommands
       {
           'xcopy /y /d /s /i /q /e "' .. AppAssets .. '" "%{cfg.targetdir}\\Assets\\"',
           'xcopy /y /d /s /i /q /e "' .. EditorRml .. '" "%{cfg.targetdir}\\Assets\\RML\\"',
       }

   -- Assets are "None" items, which Visual Studio's Fast Up-to-Date Check ignores when
   -- deciding whether to invoke MSBuild at all. Without this, editing only an asset (e.g.
   -- editor_menu.rml) with no source changes makes VS skip the build entirely, so the postbuild
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
