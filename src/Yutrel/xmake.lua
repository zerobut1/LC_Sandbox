target("Yutrel")
    set_kind("binary")
    set_rundir("$(projectdir)")

    set_default(true)

    add_files("**.cpp")
    add_headerfiles("**.h")

    add_deps("lc-dsl","lc-gui")

target_end()