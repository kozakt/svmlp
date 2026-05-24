require "export"

function addZlib(location)
  project "zlib"
    kind "StaticLib"
    language "C"

    cdialect "C17"

    files { location .. "/*.c", location .. "/src/*.h", }

    removefiles { location .. "/example.c", location .. "/minigzip.c" }

    includedirs { location .. "/" }

    export "*"
      links { "zlib" }
      includedirs { location .. "/" }
    export {}
end