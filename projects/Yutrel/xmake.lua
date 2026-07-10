target("Yutrel")
    set_kind("binary")
    set_rundir("$(projectdir)/projects/Yutrel")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")

    add_includedirs("src")

    add_deps("lc-dsl", "lc-gui", "stb-image")
    add_packages("tinyexr")
    add_packages("assimp")

target_end()