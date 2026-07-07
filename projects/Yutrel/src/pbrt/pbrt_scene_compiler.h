#pragma once

#include "base/scene.h"
#include "scene/scene_description.h"

namespace Yutrel
{

class PbrtSceneCompiler
{
public:
    [[nodiscard]] static Scene::CreateInfo compile(SceneDescription desc);
};

} // namespace Yutrel
