#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// Which stat pool a consumable restores when used -- see UseItemAction.
// RestoreHp routes through HealthSystem's IncomingHealEvent (the same
// sole-writer pattern damage already uses); RestoreTp is a direct
// TPComponent mutation instead, since no TP-owning system exists yet to
// route through (see HealEvent.h's doc comment for why the two differ).
enum class ConsumableEffect
{
    RestoreHp,
    RestoreTp
};

template <> struct EnumNames<ConsumableEffect>
{
    static constexpr std::array<std::pair<std::string_view, ConsumableEffect>, 2> kValues{{
        {"restore_hp", ConsumableEffect::RestoreHp},
        {"restore_tp", ConsumableEffect::RestoreTp},
    }};
};

// A consumable item prefab's effect -- e.g. Monomate (RestoreHp) or
// Monofluid (RestoreTp). Flat amount only, matching Affix's own "just needs
// to round-trip as data for now" precedent (no percentage-based healing, no
// over-time effects). Always paired with ItemComponent on the same prefab
// so PickupAction can pick it up like any other item.
struct ConsumableComponent
{
    ConsumableEffect effect = ConsumableEffect::RestoreHp;
    int amount = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ConsumableComponent>("consumable")
            .Data<&ConsumableComponent::effect>("effect")
            .Data<&ConsumableComponent::amount>("amount");
    }
};

} // namespace psr
