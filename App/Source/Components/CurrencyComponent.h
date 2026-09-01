#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// The player's accumulated Meseta for this run. Authored on player.json with
// a default of 0 (same shape as HealthComponent authoring a starting HP
// value); LootDropSystem is the only thing that mutates it after spawn.
struct CurrencyComponent
{
    int meseta = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<CurrencyComponent>("currency").Data<&CurrencyComponent::meseta>("meseta");
    }
};

} // namespace psr
