#pragma once

#include "pbrt/pbrt_scene.h"
#include "scene/scene_spec.h"

namespace Yutrel
{

class PbrtImporter
{
public:
    [[nodiscard]] static SceneSpec import(PbrtScene scene);
};

} // namespace Yutrel
