#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/Math/Vec2.h"

namespace psr {

// An entity's location on its Grid, in tile coordinates.
struct Position
{
    Vec2 tile;

    // Not authorable -- stamped by the engine when an entity is placed
    // (spawned/instantiated), never something content sets directly.
    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<Position>("position", /*authorable=*/false).Data<&Position::tile>("tile");
    }
};

} // namespace psr
