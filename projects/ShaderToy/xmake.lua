target("ShaderToy")
    set_kind("binary")
    set_rundir("$(projectdir)/projects/ShaderToy")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")

    add_deps("lc-dsl", "lc-gui")
target_end()
