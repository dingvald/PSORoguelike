-- Build.lua
workspace "PSORoguelike"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "App"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

-- vcpkg (manifest mode) install tree, relative to the workspace root
VcpkgDir = "vcpkg_installed/x64-windows"

group "Core"
	include "Core/Build-Core.lua"
group ""

include "App/Build-App.lua"

group "Tools"
	include "Editor/Build-Editor.lua"
group ""

group "Tests"
	include "Core-Test/Build-Core-Test.lua"
group ""
