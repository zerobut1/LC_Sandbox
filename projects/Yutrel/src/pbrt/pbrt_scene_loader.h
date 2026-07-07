#pragma once

#include <filesystem>

#include "scene/scene_description.h"

namespace Yutrel
{

class PbrtSceneLoader
{
public:
    [[nodiscard]] static SceneDescription load(const std::filesystem::path& path);
};

} // namespace Yutrel
