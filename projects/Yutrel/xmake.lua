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

target("YutrelPbrtSceneLoaderTest")
    set_kind("binary")
    set_group("tests")
    set_default(false)
    set_rundir("$(projectdir)")

    add_files("src/pbrt/pbrt_scene_loader.cpp")
    add_files("tests/pbrt_scene_loader_test.cpp")
    add_headerfiles("src/**.h")

    add_includedirs("src")

    add_deps("lc-dsl")

target_end()
