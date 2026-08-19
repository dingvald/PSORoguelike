project "Core"
   kind "StaticLib"
   language "C++"
   cppdialect "C++23"
   staticruntime "off"

   files { "Source/**.h", "Source/**.cpp" }

   includedirs
   {
      "Source",

      -- vcpkg dependencies (SDL3, SDL3_image, RmlUi, FreeType)
      ("../" .. VcpkgDir .. "/include"),
   }

   -- The vendored RmlUi SDL backend targets SDL3.
   defines { "RMLUI_SDL_VERSION_MAJOR=3" }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { "WINDOWS" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
       symbols "On"

   filter "configurations:Dist"
       defines { "DIST" }
       runtime "Release"
       optimize "On"
       symbols "Off"
