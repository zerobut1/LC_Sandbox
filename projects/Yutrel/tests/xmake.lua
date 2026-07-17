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

yutrel_test("cli_options", {
    "test_cli_options.cpp",
    "../src/cli_options.cpp",
})

local function yutrel_core_test(name, common)
    target("test_Yutrel_" .. name)
        set_kind("binary")
        set_default(false)
        set_group("tests/Yutrel")
        set_rundir("$(projectdir)/projects/Yutrel")

        add_files("test_" .. name .. ".cpp")
        add_includedirs("$(projectdir)/ext/LuisaCompute/src/tests")
        if common then
            add_includedirs("$(projectdir)/ext/LuisaCompute/src/tests/common")
        end
        add_deps("YutrelCore")
    target_end()
end

yutrel_core_test("book_import")
yutrel_core_test("robustness", true)
yutrel_core_test("coated_diffuse")
yutrel_core_test("color_space")
yutrel_core_test("environment")
yutrel_core_test("filter")
yutrel_core_test("sobol")
