#include "scene.h"

namespace Yutrel
{
struct Scene::Config
{
    luisa::unique_ptr<Camera> camera;
    luisa::unique_ptr<Film> film;
    luisa::vector<luisa::unique_ptr<Shape>> shapes;
    luisa::vector<luisa::unique_ptr<Surface>> surfaces;
    luisa::vector<luisa::unique_ptr<Light>> lights;
    luisa::vector<luisa::unique_ptr<Texture>> textures;

    luisa::vector<const Shape*> shapes_view;
};

Scene::Scene(const Context& context) noexcept
    : m_context(context),
      m_config(luisa::make_unique<Config>()) {}

Scene::~Scene() noexcept = default;

luisa::unique_ptr<Scene> Scene::create(const Context& context, const CreateInfo& info) noexcept
{
    auto scene = luisa::make_unique<Scene>(context);

    scene->load_camera(info.camera_info);

    scene->m_config->shapes_view.reserve(info.shape_infos.size());
    for (auto& shape_info : info.shape_infos)
    {
        scene->m_config->shapes_view.emplace_back(scene->load_shape(shape_info));
    }
    return scene;
}

void Scene::load_camera(const Camera::CreateInfo& info) noexcept
{
    if (m_config->camera)
    {
        LUISA_ERROR("Multiple cameras are not supported yet.");
        return;
    }
    m_config->camera = Camera::create(*this, info);
}

const Film* Scene::load_film(const Film::CreateInfo& info) noexcept
{
    if (m_config->film)
    {
        LUISA_ERROR("Multiple films are not supported yet.");
        return nullptr;
    }
    return (m_config->film = Film::create(info)).get();
}

const Shape* Scene::load_shape(const Shape::CreateInfo& info) noexcept
{
    return m_config->shapes.emplace_back(Shape::create(*this, info)).get();
}

const Surface* Scene::load_surface(const Surface::CreateInfo& info) noexcept
{
    return m_config->surfaces.emplace_back(Surface::create(*this, info)).get();
}

const Light* Scene::load_light(const Light::CreateInfo& info) noexcept
{
    return m_config->lights.emplace_back(Light::create(*this, info)).get();
}

const Texture* Scene::load_texture(const Texture::CreateInfo& info) noexcept
{
    return m_config->textures.emplace_back(Texture::create(*this, info)).get();
}

const Camera* Scene::camera() const noexcept
{
    return m_config->camera.get();
}

const Film* Scene::film() const noexcept
{
    return m_config->film.get();
}

luisa::span<const Shape* const> Scene::shapes() const noexcept
{
    return m_config->shapes_view;
}
} // namespace Yutrel