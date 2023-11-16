package = "lrtaudio"
version = "scm-0"
local versionNumber = version:gsub("^(.*)-.-$", "%1")
source = {
  url = "https://github.com/osch/lua-lrtaudio/archive/master.zip",
  dir = "lua-lrtaudio-master",
}
description = {
  summary = "Lua binding for the RtAudio",
  homepage = "https://github.com/osch/lua-lrtaudio",
  license = "MIT",
  detailed = [[
      Lua binding for the RtAudio ( https://www.music.mcgill.ca/~gary/rtaudio/ ).
      
      This binding enables Lua scripting code to open RtAudio streams and to
      connect them with Lua audio processor objects for realtime processing. 
      Realtime audio processing of the Lua processor objects has to be implemented in
      native C code. 
  ]],
}
dependencies = {
  "lua >= 5.1, <= 5.4",
  "luarocks-build-extended"
}
build = {
  type = "extended",
  platforms = {
      macosx = {
          modules = {
              lrtaudio = {
                  variables = {
                      CXXFLAG_EXTRAS = { "-std=c++11" },
                  }
              }
          }
      }
  },
  modules = {
    lrtaudio = {
      libraries = {
        "rtaudio"
      },
      sources = { 
          "src/main.cpp",
          "src/controller.cpp",
          "src/channel.cpp",
          "src/stream.cpp",
          "src/procbuf.cpp",
          "src/auproc_capi_impl.cpp",
          "src/async_util.cpp",
          "src/error.cpp",
          "src/lrtaudio_compat.c"
      },
      defines = { "LRTAUDIO_VERSION="..versionNumber },
    },
  }
}