add_rules("mode.debug", "mode.release", "mode.releasedbg")

set_defaultarchs("x64")
set_languages("c++20")

includes("ext/LuisaCompute")
includes("config.lua")
includes("src")