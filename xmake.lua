add_rules("mode.debug", "mode.release", "mode.releasedbg")

set_defaultarchs("x64")
set_languages("c++20")

includes("ext/LuisaCompute")
includes("config.lua")

target("LC_Sandbox")
    set_kind("binary")
    set_rundir("$(projectdir)")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")

    add_deps("lc-dsl","lc-gui")

target_end()