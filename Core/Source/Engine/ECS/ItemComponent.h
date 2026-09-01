#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Marks a prefab as a pickupable item -- the tag PickupAction looks for to
// tell a ground item apart from everything else that can share a tile (floor,
// wall, decoration, an actor), same shape/empty-tag precedent as
// ModComponent/BlocksMovementComponent.
struct ItemComponent
{
    static void Register(ComponentSchemaRegistrar& reg) { reg.Component<ItemComponent>("item"); }
};

} // namespace psr
