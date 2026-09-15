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
    RegistryRenderableLookup(Registry& registry, AnimationClock& animation_clock);
    ~RegistryRenderableLookup() override;

    // Binds an on_destroy<RenderableComponent> listener capturing this
    // instance's address -- neither copying nor moving would keep it valid
    // (C.21/C.81).
    RegistryRenderableLookup(const RegistryRenderableLookup&) = delete;
    RegistryRenderableLookup& operator=(const RegistryRenderableLookup&) = delete;
    RegistryRenderableLookup(RegistryRenderableLookup&&) = delete;
    RegistryRenderableLookup& operator=(RegistryRenderableLookup&&) = delete;

    std::optional<RenderableTile> GetRenderableTile(entt::entity entity) const override;
    Vec2f GetRenderOffset(entt::entity entity) const override;

private:
    void OnRenderableDestroyed(entt::registry& registry, entt::entity entity);

    Registry* m_registry;
    AnimationClock* m_animation_clock;
};

} // namespace psr
