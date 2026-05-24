require "export"
include "zlib.lua"

workspace "SVMLP"
  location(_ACTION)

  cppdialect "C++20"

  configurations { "Debug", "Release" }

  platforms { "x86", "x64" }

  targetdir ("../bin")

  staticruntime "On"

  defines { "_CRT_SECURE_NO_WARNINGS" }

  filter { "configurations:Debug" }
    targetsuffix "_d"
    defines { "DEBUG" }
    symbols "On"

  filter { "configurations:Release" }
    defines { "NDEBUG" }
    optimize "On"

  filter {}

  startproject "SVMLP"

  group "Libraries"
    addZlib("../libs/zlib")
  group ""

project "SVMLP"
    kind "WindowedApp"
    language "C++"
    
    files { "../include/**", "../src/**", "../res/*.ico", "../res/*.rc" }
    
    includedirs { "../include/" }

    import
    {
      ["zlib"] = "*",
    }
