#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Data for AiBehavior::RangedTechAtDistance (see EnemyAiSystem.h) -- an
// entity with this component melees when adjacent to its target (same
// bump-into-hostile fallback ChaseAndAttack relies on), but when the target
// is aligned on a cardinal row/column within range tiles, casts technique_id
// at it instead of closing the remaining distance. Also needs TPComponent
// (for the cast's TP cost) and a KnownTechniquesComponent entry for
// technique_id (for its tier) authored on the same prefab -- mirrors PSO's
// Hildebear stopping to cast Foie at range instead of always closing to
// melee.
struct RangedTechComponent
{
    std::uint32_t technique_id = 0;
    int range = 5;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<RangedTechComponent>("ranged_tech")
            .Data<&RangedTechComponent::technique_id>("technique_id")
            .Data<&RangedTechComponent::range>("range");
    }
};

} // namespace psr
