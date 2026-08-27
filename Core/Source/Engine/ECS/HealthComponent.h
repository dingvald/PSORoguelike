#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Current/max hit points -- theme-agnostic like StatsComponent, no default
// values beyond zero implied here; real per-entity numbers are authored
// content (Prefab Editor), not engine-decided.
struct HealthComponent
{
    int current_hp = 0;
    int max_hp = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<HealthComponent>("health")
            .Data<&HealthComponent::current_hp>("current_hp")
            .Data<&HealthComponent::max_hp>("max_hp");
    }
};

} // namespace psr
