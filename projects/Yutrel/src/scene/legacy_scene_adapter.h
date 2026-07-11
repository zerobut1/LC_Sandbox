#pragma once

#include "base/scene.h"
#include "scene/scene_spec.h"

namespace Yutrel
{
[[nodiscard]] SceneSpec make_legacy_scene_spec(const Scene::CreateInfo& info);
} // namespace Yutrel
