add_rules("mode.debug", "mode.release")

set_runtimes("MD")

set_languages("c++20")

includes("ext")

add_requires("luisa-compute", {configs = {cuda = true}})

target("Luisa_Sandbox")
    set_kind("binary")
    add_files("src/**.cpp")
    add_packages("luisa-compute")

    on_config(function (target)
        target:add("runargs", path.join(target:pkg("luisa-compute"):installdir(), "bin"), "cuda")
    end)
target_end()