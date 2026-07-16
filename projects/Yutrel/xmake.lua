target("YutrelCore")
    set_kind("static")
    add_files("src/**.cpp")
    remove_files("src/main.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", {public = true})

    add_deps("lc-dsl", "lc-gui", "stb-image")
    add_packages("tinyexr", "assimp", {public = true})
target_end()

target("Yutrel")
    set_kind("binary")
    set_rundir("$(projectdir)/projects/Yutrel")

    add_files("src/main.cpp")
    add_deps("YutrelCore")

target_end()

includes("tests")
