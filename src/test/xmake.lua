local function add_test_target(name, src_dir)
    target(name)
        set_kind("binary")
        set_rundir("$(projectdir)")

        add_files(src_dir .. "/**.cpp")
        add_headerfiles(src_dir .. "/**.h")

        add_deps("lc-dsl", "lc-gui", "stb-image")
    target_end()
end

add_test_target("ShaderToy", "ShaderToy")
add_test_target("PathTracing", "pathtracing")
add_test_target("nn", "nn")
add_test_target("nn_LC", "nn_LC")
add_test_target("MNIST", "MNIST")