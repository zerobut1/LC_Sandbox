target("Yutrel")
    set_kind("binary")
    set_rundir("$(projectdir)")

    set_default(true)

    add_files("**.cpp")
    add_headerfiles("**.h")

    add_includedirs(os.scriptdir())

    add_deps("lc-dsl","lc-gui","stb-image")
    add_packages("tinyexr")
    add_packages("assimp")

target_end()