#pragma once

#include <cmath>

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>
#include <luisa/runtime/stream.h>

#include "base/camera.h"
#include "base/sampler.h"
#include "scene/spec_base.h"
#include "utils/command_buffer.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Renderer;
class LightSampler;

class Integrator
{
public:
    struct CreateInfo
    {
        uint max_depth{10u};
        uint rr_depth{0u};
        float rr_threshold{0.95f};
    };

    class Instance
    {
    private:
        Renderer& _renderer;
        const Integrator* _integrator;
        luisa::unique_ptr<Sampler::Instance> _sampler;
        luisa::unique_ptr<LightSampler> _light_sampler;

    public:
        Instance(Renderer& renderer, CommandBuffer& command_buffer, const Integrator* integrator, const Sampler* sampler) noexcept;
        virtual ~Instance() noexcept;

        [[nodiscard]] const Renderer& renderer() const noexcept { return _renderer; }
        [[nodiscard]] Renderer& renderer() noexcept { return _renderer; }
        [[nodiscard]] Sampler::Instance* sampler() const noexcept { return _sampler.get(); }
        [[nodiscard]] LightSampler* light_sampler() const noexcept { return _light_sampler.get(); }

        template <typename T = Integrator>
            requires std::is_base_of_v<Integrator, T>
        [[nodiscard]] const T* base() const noexcept
        {
            return static_cast<const T*>(_integrator);
        }

        virtual void render(Stream& stream, bool enable_display) = 0;
        virtual void render_interactive(Stream& stream)          = 0;
    };

public:
    virtual ~Integrator() noexcept = default;

    [[nodiscard]] static luisa::unique_ptr<Integrator> create(const CreateInfo& info) noexcept;
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer, const Sampler* sampler) const noexcept = 0;
};

class PathIntegrator final : public Integrator
{
public:
    class Instance final : public Integrator::Instance
    {
    public:
        Instance(Renderer& renderer, CommandBuffer& command_buffer, const PathIntegrator* integrator, const Sampler* sampler) noexcept;

        void render(Stream& stream, bool enable_display) override;
        void render_interactive(Stream& stream) override;

    private:
        [[nodiscard]] uint max_depth() const noexcept { return base<PathIntegrator>()->max_depth(); }
        [[nodiscard]] uint rr_depth() const noexcept { return base<PathIntegrator>()->rr_depth(); }
        [[nodiscard]] float rr_threshold() const noexcept { return base<PathIntegrator>()->rr_threshold(); }
        void render_one_camera(CommandBuffer& command_buffer, Camera::Instance* camera);
        [[nodiscard]] Float3 Li(const Camera::Instance* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time) const noexcept;
    };

private:
    uint _max_depth;
    uint _rr_depth;
    float _rr_threshold;

public:
    PathIntegrator(uint max_depth, uint rr_depth, float rr_threshold) noexcept;

    [[nodiscard]] uint max_depth() const noexcept { return _max_depth; }
    [[nodiscard]] uint rr_depth() const noexcept { return _rr_depth; }
    [[nodiscard]] float rr_threshold() const noexcept { return _rr_threshold; }
    [[nodiscard]] luisa::unique_ptr<Integrator::Instance> build(Renderer& renderer, CommandBuffer& command_buffer, const Sampler* sampler) const noexcept override;
};

class PathIntegratorSpec final : public IntegratorSpec
{
private:
    uint _max_depth;
    uint _rr_depth;
    float _rr_threshold;

public:
    PathIntegratorSpec(uint max_depth, uint rr_depth, float rr_threshold) noexcept
        : _max_depth{max_depth}, _rr_depth{rr_depth}, _rr_threshold{rr_threshold} {}

    [[nodiscard]] luisa::optional<luisa::string> validate() const noexcept override
    {
        if (_max_depth == 0u)
        {
            return spec_validation_error("Path integrator max depth must be greater than zero.");
        }
        if (!std::isfinite(_rr_threshold) || _rr_threshold <= 0.0f)
        {
            return spec_validation_error("Path integrator RR threshold must be finite and positive.");
        }
        return luisa::nullopt;
    }
    [[nodiscard]] const Integrator* build(SceneBuilder& builder) const noexcept override;
};
} // namespace Yutrel
