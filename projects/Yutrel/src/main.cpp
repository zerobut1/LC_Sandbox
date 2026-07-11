#include "base/application.h"
#include "pbrt/pbrt_importer.h"
#include "pbrt/pbrt_parser.h"

#include <luisa/core/logging.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace Yutrel;

namespace
{

[[noreturn]] void fail(luisa::string message)
{
    throw std::runtime_error{message.c_str()};
}

[[noreturn]] void print_usage_and_exit(char const* bin)
{
    fail(luisa::format("Usage: {} <backend> <scene.pbrt> [--interactive|-i] [--headless]. <backend>: cuda, dx, metal", bin));
}

[[nodiscard]] bool is_interactive_arg(luisa::string_view arg) noexcept
{
    return arg == "--interactive" || arg == "-i";
}

[[nodiscard]] bool is_headless_arg(luisa::string_view arg) noexcept
{
    return arg == "--headless";
}

[[nodiscard]] bool is_pbrt_scene_path(std::filesystem::path const& path)
{
    return path.extension() == ".pbrt";
}

[[nodiscard]] SceneSpec load_pbrt_scene_spec(std::filesystem::path const& scene_path)
{
    auto scene = PbrtParser::parse(scene_path);
    return PbrtImporter::import(std::move(scene));
}

} // namespace

int main(int argc, char* argv[])
try
{
    if (argc <= 1)
    {
        print_usage_and_exit(argv[0]);
    }

    bool interactive    = false;
    bool headless       = false;
    bool has_scene_path = false;
    std::filesystem::path scene_path{};

    for (int i = 2; i < argc; i++)
    {
        auto arg = luisa::string_view{argv[i]};
        if (is_interactive_arg(arg))
        {
            interactive = true;
            continue;
        }
        if (is_headless_arg(arg))
        {
            headless = true;
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
        {
            fail(luisa::format("Unknown option '{}'. Usage: {} <backend> <scene.pbrt> [--interactive|-i] [--headless].",
                               arg,
                               argv[0]));
        }

        auto candidate = std::filesystem::path{argv[i]};
        if (!is_pbrt_scene_path(candidate))
        {
            fail(luisa::format("Unsupported scene file '{}'. Expected a .pbrt file.",
                               candidate.string()));
        }

        if (has_scene_path)
        {
            fail(luisa::format("Multiple scene files specified: '{}' and '{}'. Only one .pbrt scene path is supported.",
                               scene_path.string(),
                               candidate.string()));
        }

        scene_path     = candidate;
        has_scene_path = true;
    }

    if (!has_scene_path)
    {
        print_usage_and_exit(argv[0]);
    }
    if (interactive && headless)
    {
        fail("--interactive and --headless cannot be used together.");
    }

    ApplicationOptions options{
        .bin         = argv[0],
        .backend     = argv[1],
        .interactive = interactive,
        .headless    = headless,
        .scene       = load_pbrt_scene_spec(scene_path),
    };

    Application app{std::move(options)};
    app.run();

    return 0;
}
catch (const std::exception& e)
{
    std::cerr << "Yutrel error: " << e.what() << '\n';
    return EXIT_FAILURE;
}
catch (...)
{
    std::cerr << "Yutrel error: unknown fatal error.\n";
    return EXIT_FAILURE;
}
