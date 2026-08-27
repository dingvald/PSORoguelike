#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Marker: an item entity sitting on the ground after a drop (see
// Systems/LootDropSystem.h) rather than equipped or held in an inventory.
// Not authorable -- assigned programmatically when loot spawns, never
// authored on a prefab. No pickup/inventory system exists yet to consume
// this tag (see LootDropSystem's class doc comment); it only marks the
// entity as loot for whenever that system lands.
struct GroundItemComponent
{
    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<GroundItemComponent>("ground_item", /*authorable=*/false);
    }
};

} // namespace psr
