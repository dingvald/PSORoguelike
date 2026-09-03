#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace psr {

// Which stat pool a consumable restores when used, or -- for TeachTechnique --
// which Technique it teaches -- see UseItemAction. RestoreHp routes through
// HealthSystem's IncomingHealEvent (the same sole-writer pattern damage
// already uses); RestoreTp is a direct TPComponent mutation instead, since no
// TP-owning system exists yet to route through (see HealEvent.h's doc
// comment for why the two differ). TeachTechnique routes through
// Items/TechniqueLearning.h's LearnTechnique, emplacing/updating the actor's
// KnownTechniquesComponent.
enum class ConsumableEffect
{
    RestoreHp,
    RestoreTp,
    TeachTechnique
};

template <> struct EnumNames<ConsumableEffect>
{
    static constexpr std::array<std::pair<std::string_view, ConsumableEffect>, 3> kValues{{
        {"restore_hp", ConsumableEffect::RestoreHp},
        {"restore_tp", ConsumableEffect::RestoreTp},
        {"teach_technique", ConsumableEffect::TeachTechnique},
    }};
};

// A consumable item prefab's effect -- e.g. Monomate (RestoreHp), Monofluid
// (RestoreTp), or a technique disk (TeachTechnique). Flat amount only,
// matching Affix's own "just needs to round-trip as data for now" precedent
// (no percentage-based healing, no over-time effects) -- amount is reused to
// mean "tier taught" for TeachTechnique, the same overloaded-by-effect shape
// it already has between RestoreHp/RestoreTp, rather than adding a
// rarely-used dedicated field. technique_id (NameId into TechniqueLibrary) is
// only meaningful for TeachTechnique. Always paired with ItemComponent on the
// same prefab so PickupAction can pick it up like any other item.
struct ConsumableComponent
{
    ConsumableEffect effect = ConsumableEffect::RestoreHp;
    int amount = 0;
    std::uint32_t technique_id = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ConsumableComponent>("consumable")
            .Data<&ConsumableComponent::effect>("effect")
            .Data<&ConsumableComponent::amount>("amount")
            .Data<&ConsumableComponent::technique_id>("technique_id");
    }
};

} // namespace psr
