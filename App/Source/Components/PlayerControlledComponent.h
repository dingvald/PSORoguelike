#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Marker distinguishing the player-controlled actor in the turn queue from
// every other (currently AI-less, Wait-only) actor -- see TurnCoordinator.
struct PlayerControlledComponent
{
    // Not authorable -- assigned programmatically when an actor is spawned
    // as the player, not authored on a prefab (no character-creation flow
    // exists yet to assign this from content, per M10.3 being far
    // downstream).
    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<PlayerControlledComponent>("player_controlled", /*authorable=*/false);
    }
};

} // namespace psr
