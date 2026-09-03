#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// How much XP ExperienceSystem credits to the player on landing the killing
// blow against this entity -- authored directly on enemy prefabs, same
// "bespoke, no separate library lookup" shape as DropTableComponent. Entities
// without this component (most non-enemy entities, e.g. breakable boxes)
// simply grant no XP -- ExperienceSystem no-ops silently, same contract
// DropTableComponent already has for loot.
struct ExperienceValueComponent
{
    int xp = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ExperienceValueComponent>("experience_value").Data<&ExperienceValueComponent::xp>("xp");
    }
};

} // namespace psr
