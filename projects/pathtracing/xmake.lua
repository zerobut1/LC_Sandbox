target("pathtracing")
    set_kind("binary")
    set_rundir("$(projectdir)/projects/pathtracing")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")

    add_deps("lc-dsl", "lc-gui", "stb-image")
target_end()
