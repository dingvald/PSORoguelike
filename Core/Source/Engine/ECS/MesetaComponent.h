#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// A character's running Meseta total (docs/GDD.md's currency: "drops
// commonly, spent at hub shops"). Not authorable -- a running total starts
// at 0 and is never meaningful to hand-author onto a prefab, mirrors
// EnergyComponent's own "engine-managed, never content-set" convention.
// Credited by LootDropSystem on an enemy's death (see DropResolution.h).
struct MesetaComponent
{
    int amount = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<MesetaComponent>("meseta", /*authorable=*/false).Data<&MesetaComponent::amount>("amount");
    }
};

} // namespace psr
