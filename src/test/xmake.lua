target("ShaderToy")
    set_kind("binary")
    set_rundir("$(projectdir)")

    add_files("ShaderToy/**.cpp")
    add_headerfiles("ShaderToy/**.h")

    add_deps("lc-dsl","lc-gui")

target_end()

target("PathTracing")
    set_kind("binary")
    set_rundir("$(projectdir)")

    add_files("PathTracing/**.cpp")
    add_headerfiles("PathTracing/**.h")

    add_deps("lc-dsl","lc-gui")

target_end()

target("nn")
    set_kind("binary")
    set_rundir("$(projectdir)")

    add_files("nn/**.cpp")

    add_deps("lc-dsl","lc-gui")

target_end()

target("nn_LC")
    set_kind("binary")
    set_rundir("$(projectdir)")

    add_files("nn_LC/**.cpp")

    add_deps("lc-dsl","lc-gui")

target_end()