#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Marks an ItemComponent-tagged ground entity as a Meseta pile: PickupAction
// special-cases this pairing to credit CurrencyComponent::meseta directly
// (never entering InventoryComponent) instead of the normal item pickup
// path. amount is authored as a base/default value, but LootDropSystem
// overwrites it per spawn with the table's own rolled amount.
struct CurrencyPickupComponent
{
    int amount = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<CurrencyPickupComponent>("currency_pickup").Data<&CurrencyPickupComponent::amount>("amount");
    }
};

} // namespace psr
