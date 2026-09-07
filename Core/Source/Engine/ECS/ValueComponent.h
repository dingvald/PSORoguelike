#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// An item's intrinsic sell value -- what Items/Shop.h's SellItem credits the
// actor's CurrencyComponent with, independent of any shop catalog's own buy
// price (see Shop/ShopStock.h's ShopStockEntry::buy_price) -- the two are
// separately authored numbers, not one derived via a markup multiplier, per
// the user's explicit choice. Same shape/precedent as RarityComponent: one
// int field, no upper bound enforced here (a future balancing pass' job).
struct ValueComponent
{
    int base_price = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ValueComponent>("value").Data<&ValueComponent::base_price>("base_price");
    }
};

} // namespace psr
