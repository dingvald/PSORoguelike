#pragma once

#include <entt/entt.hpp>

namespace psr {

// Reacts to DeathEvent dispatched by HealthSystem at an entity whose
// HealthComponent::current_hp just hit 0: removes it from the Grid
// (Registry::GetGrid(), if it has a Position -- not every HealthComponent-
// bearing entity is necessarily grid-placed) and destroys it. The single
// place that finalizes a death, replacing the m_grid->RemoveEntity/
// registry.DestroyEntity pair every damage-dealing call site used to
// duplicate inline (and, for self-target/status-effect deaths, sometimes
// skip entirely).
//
// Stateless, wired to HealthComponent's construct/destroy lifecycle via
// Registry::BindSystemEvents<HealthComponent, DeathSystem>() -- see
// HealthSystem.h for why this isn't HealthComponent's own AttachHandlers.
class DeathSystem
{
public:
    static void AttachHandlers(entt::registry& registry, entt::entity entity);
    static void DetachHandlers(entt::registry& registry, entt::entity entity);
};

} // namespace psr
