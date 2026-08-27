project "App-Test"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++23"
   staticruntime "off"

   files
   {
      "Source/**.h", "Source/**.cpp",

      -- App is a ConsoleApp (not a library) so it can't be linked; compile its
      -- pure-logic sources directly instead, same trick Editor/Build-Editor.lua
      -- already uses for Components/Render. main.cpp and anything touching
      -- Layer/SDL/RmlUi (HelloWorldLayer, the GPU render pipeline) is out of scope.
      "../App/Source/Actions/**.h", "../App/Source/Actions/**.cpp",
      "../App/Source/Combat/**.h", "../App/Source/Combat/**.cpp",
      "../App/Source/Components/**.h", "../App/Source/Components/**.cpp",
      "../App/Source/Content/**.h", "../App/Source/Content/**.cpp",
      "../App/Source/Systems/**.h", "../App/Source/Systems/**.cpp",
      -- States/ is SDL/RmlUi-free (TargetSelectionState.cpp's own SDL3/
      -- SDL_keycode.h include is just key-code constants, no windowing/
      -- rendering link needed) -- same pure-logic-only bar as the folders above.
      "../App/Source/States/**.h", "../App/Source/States/**.cpp",
   }

   includedirs
   {
      "Source",
      "../App/Source",

      -- Include Core (the engine) -- MoveAction/Grid/Registry etc. are Core
      -- types; App-Test's own sources stay SDL/RmlUi-free (see the files
      -- list above), so this only pulls in Core's pure-logic side.
      "../Core/Source",

      -- Catch2 lives in the vcpkg include tree.
      ("../" .. VcpkgDir .. "/include"),
   }

   -- Libraries common to all configurations (same name in debug/release vcpkg trees).
   links { "Core" }

   -- Same C1128 fix App/Build-App.lua already applies to RegisterComponents.cpp's
   -- growing entt::meta registration templates.
   filter "toolset:msc*"
       buildoptions { "/bigobj" }
   filter {}

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { "WINDOWS" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"
       -- Catch2's "WithMain" library (Catch2Main) ships in a manual-link
       -- subfolder so it doesn't get pulled in by find_package() consumers.
       libdirs { ("../" .. VcpkgDir .. "/debug/lib"), ("../" .. VcpkgDir .. "/debug/lib/manual-link") }
       links { "Catch2d", "Catch2Maind" }

   filter "configurations:Release or configurations:Dist"
       runtime "Release"
       optimize "On"
       libdirs { ("../" .. VcpkgDir .. "/lib"), ("../" .. VcpkgDir .. "/lib/manual-link") }
       links { "Catch2", "Catch2Main" }

   filter "configurations:Release"
       defines { "RELEASE" }
       symbols "On"

   filter "configurations:Dist"
       defines { "DIST" }
       symbols "Off"
