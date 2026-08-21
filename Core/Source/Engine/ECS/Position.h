#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/Math/Vec2.h"

namespace psr {

// An entity's location on its Grid, in tile coordinates.
struct Position
{
    Vec2 tile;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<Position>("position").Data<&Position::tile>("tile");
    }
};

} // namespace psr
