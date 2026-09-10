#pragma once

#include "Engine/ECS/Registry.h"

namespace psr {

// Generic, theme-agnostic "expire after N in-game turns" system -- the
// turn-counted sibling of VisualEffectSystem's wall-clock Update(delta_time).
// Tick() is meant to be called exactly once per elapsed in-game turn (see
// TurnCoordinator::SetOnTurnPassed), never per frame. One first consumer:
// GameplayLayer's enemy-spawn VFX entity, stamped with LifetimeComponent{1}
// so it's visible for exactly the turn its enemy spawned in.
class LifetimeSystem
{
public:
    explicit LifetimeSystem(Registry& registry) : m_registry(&registry) {}

    // Decrements every live LifetimeComponent::remaining_turns by one;
    // destroys (and, if it has a Position, removes from the Grid) any entity
    // whose count reaches 0. Collects expired entities first, same "don't
    // mutate mid-iteration" reasoning Each<T>'s own doc comment gives.
    void Tick();

private:
    Registry* m_registry;
};

} // namespace psr
