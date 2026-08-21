#include "Render/RegistryRenderableLookup.h"

#include "Components/RenderableComponent.h"

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

} // namespace psr
