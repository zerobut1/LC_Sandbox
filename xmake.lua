add_rules("mode.debug", "mode.release", "mode.releasedbg")

set_defaultarchs("x64")
set_languages("cxx20")
set_encodings("utf-8")

add_requires("tinyexr")
add_requires("assimp")

includes("ext/LuisaCompute")
includes("src")

lc_options = {
    lc_cuda_backend = true,
    lc_dx_backend = true,
    lc_vk_backend = false,
    lc_metal_backend = false,
    lc_enable_gui = true,
    lc_enable_osl = false,
    lc_enable_dsl = true,
    lc_enable_tests = false,
    lc_enable_mimalloc = true,
    lc_enable_simd = true,
    lc_enable_unity_build = true,
    lc_dx_cuda_interop = true,
}