#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/Items/MaterialStat.h"

namespace psr {

// Marks an item prefab as a Photon crystal / stat material (docs/GDD.md --
// see MaterialStat.h). Deliberately minimal, same "just needs to round-trip
// as data for now" reasoning M8.1's Affix carried before anything consumed
// it: the actual consumable-use flow (an item-use action triggered from an
// inventory) is still M8.1's own deferred scope -- no inventory/item-use
// system exists yet, so this component only authors *what* a material item
// would do (see Engine/Items/MaterialApplication.h for the pure effect
// math), not *when* it's applied.
struct MaterialComponent
{
    MaterialStat stat = MaterialStat::Atp;
    int amount = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<MaterialComponent>("material")
            .Data<&MaterialComponent::stat>("stat")
            .Data<&MaterialComponent::amount>("amount");
    }
};

} // namespace psr
