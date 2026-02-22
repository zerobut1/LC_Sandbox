add_rules("mode.debug", "mode.release", "mode.releasedbg")

set_languages("cxx20")
set_encodings("utf-8")

add_requires("tinyexr")
add_requires("assimp")

local lc_options_win = {
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

local lc_options_macos = {
    lc_cuda_backend = false,
    lc_dx_backend = false,
    lc_vk_backend = false,
    lc_metal_backend = true,
    lc_enable_gui = true,
    lc_enable_osl = false,
    lc_enable_dsl = true,
    lc_enable_tests = false,
    lc_enable_mimalloc = true,
    lc_enable_simd = true,
    lc_enable_unity_build = true,
}

if is_plat("windows") then
    lc_options = lc_options_win
elseif is_plat("macosx") then
    lc_options = lc_options_macos
end

includes("ext/LuisaCompute")
includes("src")