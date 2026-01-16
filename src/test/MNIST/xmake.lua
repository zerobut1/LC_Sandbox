target("MNIST")
    set_kind("binary")
    set_rundir("$(projectdir)")

    add_files("**.cpp")

    add_deps("lc-dsl")
target_end()