target("MNIST")
    set_kind("binary")
    set_rundir("$(projectdir)/projects/MNIST")

    add_files("src/**.cpp")

    add_deps("lc-dsl")
target_end()
