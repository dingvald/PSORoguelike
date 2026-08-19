project "Core-Test"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++23"
   staticruntime "off"

   files { "Source/**.h", "Source/**.cpp" }

   includedirs
   {
      "Source",

      -- Include Core (the engine) -- these tests only touch pure-logic
      -- headers (Messages/Events), so SDL3/RmlUi aren't needed, but Catch2
      -- itself lives in the vcpkg include tree.
      "../Core/Source",
      ("../" .. VcpkgDir .. "/include"),
   }

   -- Libraries common to all configurations (same name in debug/release vcpkg trees).
   links { "Core" }

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
