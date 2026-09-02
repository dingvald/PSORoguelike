#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// How much EXP a monster/boss prefab grants the player on death, same shape/
// convention as DropTableComponent::drop_table_id. An entity with no
// ExperienceRewardComponent (or exp_reward <= 0) simply grants nothing --
// ExperienceSystem no-ops.
struct ExperienceRewardComponent
{
    int exp_reward = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ExperienceRewardComponent>("experience_reward")
            .Data<&ExperienceRewardComponent::exp_reward>("exp_reward");
    }
};

} // namespace psr
