#pragma once

#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/Entity.h"

namespace psr {

// Minimal placeholder until a real faction system exists: the player and
// everyone else are mutually hostile, mirroring TurnCoordinator's own
// existing player-vs-everyone-else simplification.
inline bool IsHostile(Entity a, Entity b)
{
    return a.Has<PlayerControlledComponent>() != b.Has<PlayerControlledComponent>();
}

} // namespace psr
