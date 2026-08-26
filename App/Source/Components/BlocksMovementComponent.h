#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Marker: an entity occupying this tile prevents MoveAction from moving
// another actor onto it. Authorable so content (walls, obstacles) can opt
// in via the Prefab Editor.
struct BlocksMovementComponent
{
    static void Register(ComponentSchemaRegistrar& reg) { reg.Component<BlocksMovementComponent>("blocks_movement"); }
};

} // namespace psr
