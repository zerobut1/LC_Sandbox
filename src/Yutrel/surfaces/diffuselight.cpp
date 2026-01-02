#include "diffuselight.h"

#include "base/renderer.h"
#include "base/scene.h"

namespace Yutrel
{
DiffuseLight::DiffuseLight(Scene& scene, const CreateInfo& info) noexcept
    : Surface(scene, info), m_emit(scene.load_texture(info.emit)) {}

luisa::unique_ptr<Surface::Instance> DiffuseLight::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto emit = renderer.build_texture(command_buffer, m_emit);

    return luisa::make_unique<Instance>(renderer, this, emit);
}

luisa::unique_ptr<Surface::Closure> DiffuseLight::Instance::create_closure(Expr<float> time) const noexcept
{
    return luisa::make_unique<Closure>(renderer(), time);
}

void DiffuseLight::Instance::populate_closure(Surface::Closure* closure, const Interaction& it) const noexcept
{
    Float3 emit = m_emit->evaluate(it).xyz();

    DiffuseLight::Closure::Context ctx{
        .it   = it,
        .emit = emit,
    };
    closure->bind(std::move(ctx));
}

Float3 DiffuseLight::Closure::emitted() const noexcept
{
    auto&& ctx = context<Context>();

    return ite(ctx.it.front_face, ctx.emit, make_float3(0.0f));
}

} // namespace Yutrel