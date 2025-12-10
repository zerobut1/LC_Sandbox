#include "core/application.h"

#include <luisa/core/logging.h>

using namespace Yutrel;

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        LUISA_INFO("Usage: {} <backend>. <backend>: cuda, dx, vk", argv[0]);
        exit(1);
    }

    ApplicationCreateInfo app_info{
        .bin         = argv[0],
        .backend     = argv[1],
        .window_name = "Yutrel",
        .resolution  = uint2{1920u, 1080u},
    };

    Application app{app_info};
    app.run();

    return 0;
}