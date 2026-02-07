add_rules("mode.debug", "mode.release", "mode.releasedbg")

set_languages("cxx20")
set_encodings("utf-8")

add_requires("tinyexr")
add_requires("assimp")

if is_host("windows") then
    lc_options = {
        lc_cuda_backend = true,
        lc_dx_backend = true,
        lc_vk_backend = false,
        lc_metal_backend = false,
        lc_enable_gui = true,
        lc_enable_tests = false,
        lc_enable_osl = false,
    }
elseif is_host("macosx") then
    lc_options = {
        lc_cuda_backend = false,
        lc_dx_backend = false,
        lc_vk_backend = false,
        lc_metal_backend = true,
        lc_enable_gui = true,
        lc_enable_tests = false,
        lc_enable_osl = false,
    }
end

includes("ext/LuisaCompute")
includes("src")