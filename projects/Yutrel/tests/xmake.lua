local function yutrel_test(name, files)
    target("test_Yutrel_" .. name)
        set_kind("binary")
        set_default(false)
        set_group("tests/Yutrel")
        set_rundir("$(projectdir)/projects/Yutrel")

        add_files(table.unpack(files))
        add_includedirs("../src", "$(projectdir)/ext/LuisaCompute/src/tests")
        add_deps("lc-core")
    target_end()
end

yutrel_test("book_parse", {
    "test_book_parse.cpp",
    "../src/pbrt/pbrt_parser.cpp",
})

target("test_Yutrel_book_import")
    set_kind("binary")
    set_default(false)
    set_group("tests/Yutrel")
    set_rundir("$(projectdir)/projects/Yutrel")

    add_files("test_book_import.cpp", "../src/**.cpp")
    remove_files("../src/main.cpp")
    add_includedirs("../src", "$(projectdir)/ext/LuisaCompute/src/tests")
    add_deps("lc-dsl", "lc-gui", "stb-image")
    add_packages("tinyexr", "assimp")
target_end()

target("test_Yutrel_coated_diffuse")
    set_kind("binary")
    set_default(false)
    set_group("tests/Yutrel")
    set_rundir("$(projectdir)/projects/Yutrel")

    add_files("test_coated_diffuse.cpp", "../src/**.cpp")
    remove_files("../src/main.cpp")
    add_includedirs("../src", "$(projectdir)/ext/LuisaCompute/src/tests")
    add_deps("lc-dsl", "lc-gui", "stb-image")
    add_packages("tinyexr", "assimp")
target_end()

target("test_Yutrel_environment")
    set_kind("binary")
    set_default(false)
    set_group("tests/Yutrel")
    set_rundir("$(projectdir)/projects/Yutrel")

    add_files("test_environment.cpp", "../src/**.cpp")
    remove_files("../src/main.cpp")
    add_includedirs("../src", "$(projectdir)/ext/LuisaCompute/src/tests")
    add_deps("lc-dsl", "lc-gui", "stb-image")
    add_packages("tinyexr", "assimp")
target_end()
