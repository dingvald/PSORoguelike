#pragma once

#include "Engine/ECS/Registry.h"
#include "Engine/Render/IRenderableLookup.h"

namespace psr {

// App-side IRenderableLookup: resolves a grid-tile entity's RenderableComponent
// (App content) into the RenderableTile TileRenderer (Core mechanism) draws.
class RegistryRenderableLookup : public IRenderableLookup
{
public:
    explicit RegistryRenderableLookup(Registry& registry);

    std::optional<RenderableTile> GetRenderableTile(entt::entity entity) const override;
    Vec2f GetRenderOffset(entt::entity entity) const override;

private:
    Registry* m_registry;
};

} // namespace psr
