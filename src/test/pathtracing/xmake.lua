target("pathtracing")
    set_kind("binary")
    set_rundir("$(projectdir)/src/test/pathtracing")

    add_files("**.cpp")
    add_headerfiles("**.h")

    add_deps("lc-dsl", "lc-gui", "stb-image")
target_end()