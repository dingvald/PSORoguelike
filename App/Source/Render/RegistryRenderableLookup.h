#pragma once

#include "Engine/ECS/Registry.h"
#include "Engine/Render/AnimationClock.h"
#include "Engine/Render/IRenderableLookup.h"

namespace psr {

// App-side IRenderableLookup: resolves a grid-tile entity's RenderableComponent
// (App content) into the RenderableTile TileRenderer (Core mechanism) draws.
class RegistryRenderableLookup : public IRenderableLookup
{
public:
    RegistryRenderableLookup(Registry& registry, const AnimationClock& animation_clock);

    std::optional<RenderableTile> GetRenderableTile(entt::entity entity) const override;
    Vec2f GetRenderOffset(entt::entity entity) const override;

private:
    Registry* m_registry;
    const AnimationClock* m_animation_clock;
};

} // namespace psr
