#include "Render/RegistryRenderableLookup.h"

#include "Components/RenderableComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/Math/Easing.h"

#include <algorithm>

namespace psr {

RegistryRenderableLookup::RegistryRenderableLookup(Registry& registry) : m_registry(&registry) {}

std::optional<RenderableTile> RegistryRenderableLookup::GetRenderableTile(entt::entity entity) const
{
    const RenderableComponent* component = m_registry->TryGetComponent<RenderableComponent>(entity);
    if (!component)
        return std::nullopt;

    return RenderableTile{component->texture_id, component->texture_size, component->uv,
                          component->color_1,    component->color_2,      component->render_layer};
}

Vec2f RegistryRenderableLookup::GetRenderOffset(entt::entity entity) const
{
    const TweenComponent* tween_component = m_registry->TryGetComponent<TweenComponent>(entity);
    if (!tween_component || tween_component->queue.empty())
        return {};

    const Tween& active = tween_component->queue.front();
    const float progress = active.duration > 0.0f ? std::clamp(active.elapsed / active.duration, 0.0f, 1.0f) : 1.0f;
    return active.start_offset + (active.end_offset - active.start_offset) * EaseOutQuad(progress);
}

} // namespace psr
