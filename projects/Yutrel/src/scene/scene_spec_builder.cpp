#include "scene/scene_spec_builder.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace Yutrel
{
namespace
{

enum class SpecCategory : uint8_t
{
    Texture,
    Surface,
    Light,
    Shape,
    Spectrum,
    Camera,
    Film,
    Filter,
    Sampler,
    Integrator,
    Count,
};

struct SpecNode
{
    SpecCategory category;
    uint32_t index;
};

struct SpecNodeData
{
    SpecCategory category;
    SpecMeta meta;
    luisa::vector<SpecNode> dependencies;
};

class DependencyCollector final : public SpecDependencyVisitor
{
private:
    luisa::vector<SpecNode>& _dependencies;

public:
    explicit DependencyCollector(luisa::vector<SpecNode>& dependencies) noexcept
        : _dependencies{dependencies}
    {
    }

    void visit(TextureRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Texture, ref.index()}); }
    void visit(SurfaceRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Surface, ref.index()}); }
    void visit(LightRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Light, ref.index()}); }
    void visit(ShapeRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Shape, ref.index()}); }
    void visit(SpectrumRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Spectrum, ref.index()}); }
    void visit(CameraRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Camera, ref.index()}); }
    void visit(FilmRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Film, ref.index()}); }
    void visit(FilterRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Filter, ref.index()}); }
    void visit(SamplerRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Sampler, ref.index()}); }
    void visit(IntegratorRef ref) noexcept override { _dependencies.emplace_back(SpecNode{SpecCategory::Integrator, ref.index()}); }
};

[[nodiscard]] constexpr size_t category_index(SpecCategory category) noexcept
{
    return static_cast<size_t>(category);
}

[[nodiscard]] constexpr luisa::string_view category_name(SpecCategory category) noexcept
{
    switch (category)
    {
    case SpecCategory::Texture:
        return "texture";
    case SpecCategory::Surface:
        return "surface";
    case SpecCategory::Light:
        return "light";
    case SpecCategory::Shape:
        return "shape";
    case SpecCategory::Spectrum:
        return "spectrum";
    case SpecCategory::Camera:
        return "camera";
    case SpecCategory::Film:
        return "film";
    case SpecCategory::Filter:
        return "filter";
    case SpecCategory::Sampler:
        return "sampler";
    case SpecCategory::Integrator:
        return "integrator";
    case SpecCategory::Count:
        break;
    }
    return "unknown";
}

[[noreturn]] void throw_validation_error(const SourceLocation& source, luisa::string message)
{
    auto formatted = luisa::format("{}: {}", format_source_location(source), message);
    throw std::runtime_error{formatted.c_str()};
}

} // namespace

TextureRef SceneSpecBuilder::reference_texture(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _textures.reference(std::move(name), std::move(use_site));
}

SurfaceRef SceneSpecBuilder::reference_surface(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _surfaces.reference(std::move(name), std::move(use_site));
}

LightRef SceneSpecBuilder::reference_light(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _lights.reference(std::move(name), std::move(use_site));
}

ShapeRef SceneSpecBuilder::reference_shape(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _shapes.reference(std::move(name), std::move(use_site));
}

SpectrumRef SceneSpecBuilder::reference_spectrum(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _spectra.reference(std::move(name), std::move(use_site));
}

CameraRef SceneSpecBuilder::reference_camera(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _cameras.reference(std::move(name), std::move(use_site));
}

FilmRef SceneSpecBuilder::reference_film(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _films.reference(std::move(name), std::move(use_site));
}

FilterRef SceneSpecBuilder::reference_filter(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _filters.reference(std::move(name), std::move(use_site));
}

SamplerRef SceneSpecBuilder::reference_sampler(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _samplers.reference(std::move(name), std::move(use_site));
}

IntegratorRef SceneSpecBuilder::reference_integrator(luisa::string name, SourceLocation use_site)
{
    _ensure_mutable();
    return _integrators.reference(std::move(name), std::move(use_site));
}

SceneSpec SceneSpecBuilder::finish()
{
    _ensure_mutable();
    _validate();
    _finished = true;
    return SceneSpec{
        std::move(_textures),
        std::move(_surfaces),
        std::move(_lights),
        std::move(_shapes),
        std::move(_spectra),
        std::move(_cameras),
        std::move(_films),
        std::move(_filters),
        std::move(_samplers),
        std::move(_integrators),
    };
}

void SceneSpecBuilder::_ensure_mutable() const
{
    if (_finished)
    {
        throw std::runtime_error{"SceneSpecBuilder has already been finished."};
    }
}

void SceneSpecBuilder::_validate() const
{
    _textures.validate_definitions();
    _surfaces.validate_definitions();
    _lights.validate_definitions();
    _shapes.validate_definitions();
    _spectra.validate_definitions();
    _cameras.validate_definitions();
    _films.validate_definitions();
    _filters.validate_definitions();
    _samplers.validate_definitions();
    _integrators.validate_definitions();

    constexpr auto category_count = category_index(SpecCategory::Count);
    std::array<size_t, category_count> counts{
        _textures.size(),
        _surfaces.size(),
        _lights.size(),
        _shapes.size(),
        _spectra.size(),
        _cameras.size(),
        _films.size(),
        _filters.size(),
        _samplers.size(),
        _integrators.size(),
    };
    std::array<size_t, category_count> offsets{};
    for (size_t i = 1u; i < category_count; i++)
    {
        offsets[i] = offsets[i - 1u] + counts[i - 1u];
    }
    auto total_count = offsets.back() + counts.back();

    luisa::vector<SpecNodeData> nodes;
    nodes.reserve(total_count);
    auto append_table = [&]<typename Spec>(SpecCategory category, const SpecTable<Spec>& table)
    {
        table.visit_entries(
            [&](SceneRef<Spec>, const SpecMeta& meta, const Spec* spec)
        {
            SpecNodeData node{.category = category, .meta = meta};
            DependencyCollector collector{node.dependencies};
            spec->visit_dependencies(collector);
            nodes.emplace_back(std::move(node));
        });
        if (nodes.size() != offsets[category_index(category)] + counts[category_index(category)])
        {
            throw std::runtime_error{"Internal SceneSpec category layout mismatch."};
        }
    };

    append_table(SpecCategory::Texture, _textures);
    append_table(SpecCategory::Surface, _surfaces);
    append_table(SpecCategory::Light, _lights);
    append_table(SpecCategory::Shape, _shapes);
    append_table(SpecCategory::Spectrum, _spectra);
    append_table(SpecCategory::Camera, _cameras);
    append_table(SpecCategory::Film, _films);
    append_table(SpecCategory::Filter, _filters);
    append_table(SpecCategory::Sampler, _samplers);
    append_table(SpecCategory::Integrator, _integrators);

    auto dependency_exists = [&](SpecNode node) noexcept
    {
        switch (node.category)
        {
        case SpecCategory::Texture:
            return _textures.contains_index(node.index);
        case SpecCategory::Surface:
            return _surfaces.contains_index(node.index);
        case SpecCategory::Light:
            return _lights.contains_index(node.index);
        case SpecCategory::Shape:
            return _shapes.contains_index(node.index);
        case SpecCategory::Spectrum:
            return _spectra.contains_index(node.index);
        case SpecCategory::Camera:
            return _cameras.contains_index(node.index);
        case SpecCategory::Film:
            return _films.contains_index(node.index);
        case SpecCategory::Filter:
            return _filters.contains_index(node.index);
        case SpecCategory::Sampler:
            return _samplers.contains_index(node.index);
        case SpecCategory::Integrator:
            return _integrators.contains_index(node.index);
        case SpecCategory::Count:
            return false;
        }
        return false;
    };
    auto flat_index = [&](SpecNode node) noexcept
    {
        return offsets[category_index(node.category)] + node.index;
    };

    for (auto& node : nodes)
    {
        for (auto dependency : node.dependencies)
        {
            if (!dependency_exists(dependency))
            {
                throw_validation_error(
                    node.meta.source,
                    luisa::format(
                        "{} spec '{}' references an out-of-bounds {} spec index {}.",
                        category_name(node.category),
                        node.meta.name,
                        category_name(dependency.category),
                        dependency.index));
            }
        }
    }

    enum class VisitState : uint8_t
    {
        Unvisited,
        Visiting,
        Visited,
    };
    luisa::vector<VisitState> states(nodes.size(), VisitState::Unvisited);
    luisa::vector<size_t> stack;
    auto visit = [&](auto&& self, size_t node_index) -> void
    {
        states[node_index] = VisitState::Visiting;
        stack.emplace_back(node_index);
        for (auto dependency : nodes[node_index].dependencies)
        {
            auto dependency_index = flat_index(dependency);
            if (states[dependency_index] == VisitState::Unvisited)
            {
                self(self, dependency_index);
            }
            else if (states[dependency_index] == VisitState::Visiting)
            {
                auto cycle_begin = std::find(stack.begin(), stack.end(), dependency_index);
                luisa::string cycle;
                for (auto iter = cycle_begin; iter != stack.end(); ++iter)
                {
                    if (!cycle.empty())
                    {
                        cycle.append(" -> ");
                    }
                    cycle.append(luisa::format("{} '{}'", category_name(nodes[*iter].category), nodes[*iter].meta.name));
                }
                cycle.append(luisa::format(" -> {} '{}'", category_name(nodes[dependency_index].category), nodes[dependency_index].meta.name));
                throw_validation_error(nodes[dependency_index].meta.source, luisa::format("Spec dependency cycle: {}.", cycle));
            }
        }
        stack.pop_back();
        states[node_index] = VisitState::Visited;
    };

    for (size_t i = 0u; i < nodes.size(); i++)
    {
        if (states[i] == VisitState::Unvisited)
        {
            visit(visit, i);
        }
    }
}

} // namespace Yutrel
