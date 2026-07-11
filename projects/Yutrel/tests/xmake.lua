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
