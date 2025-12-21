add_rules("mode.debug", "mode.release", "mode.releasedbg")

set_defaultarchs("x64")
set_languages("cxx20")
set_encodings("utf-8")

includes("ext/LuisaCompute")
includes("config.lua")
includes("src")