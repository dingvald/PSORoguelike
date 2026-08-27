#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Star rarity, shared verbatim across weapon/armor/mod prefabs (M8.1) -- one
// component, three roles, rather than duplicating a single int field into
// three near-identical structs. No upper bound is enforced here: exact
// tiering is a future balancing pass, same deferral this project already
// makes for other authored numbers (see StatsComponent).
struct RarityComponent
{
    int stars = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<RarityComponent>("rarity").Data<&RarityComponent::stars>("stars");
    }
};

} // namespace psr
