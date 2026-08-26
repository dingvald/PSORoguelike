#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// An actor's persisted scheduling energy -- the TurnQueue source of truth
// (TurnQueue itself only holds the transient scheduling copy, resynced via
// Requeue after every turn). Presence/absence of this component is what
// drives TurnQueue membership (see TurnCoordinator).
struct EnergyComponent
{
    int energy = 0;

    // Not authorable -- engine-managed scheduling state, never something
    // content sets directly (mirrors Position's convention).
    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<EnergyComponent>("energy", /*authorable=*/false).Data<&EnergyComponent::energy>("energy");
    }
};

} // namespace psr
