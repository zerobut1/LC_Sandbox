#include "base/application.h"
#include "cli_options.h"
#include "pbrt/pbrt_importer.h"
#include "pbrt/pbrt_parser.h"

#include <cstdlib>
#include <iostream>
#include <utility>

using namespace Yutrel;

int main(int argc, char* argv[])
try
{
    auto cli        = parse_command_line(argc, argv);
    auto pbrt_scene = PbrtParser::parse(cli.scene_path);
    apply_cli_overrides(pbrt_scene, cli.overrides);
    auto scene = PbrtImporter::import(std::move(pbrt_scene));
    ApplicationOptions options{
        .bin         = argv[0],
        .backend     = cli.backend,
        .interactive = cli.interactive,
        .headless    = cli.headless,
    };

    Application app{std::move(options), scene};
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
