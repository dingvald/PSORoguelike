#pragma once

#include "Combat/Element.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h" // WeaponRangeShape, RaceBonusEntry
#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <vector>

namespace psr {

// Dispatched by AttackAction to the actor's own EventHandlerComponent
// (Entity::Dispatch) at the very start of Perform(), before AttackAction
// touches any component itself. EquipmentComponent's own AttachHandlers-
// registered handler resolves the equipped weapon (if any) and fills
// has_weapon/range_shape/range/hits_per_turn/race_bonuses/attacker_stats/
// element/status_effect_id/status_chance_percent -- AttackAction never reads
// EquipmentComponent/WeaponComponent directly. StatusEffectComponent's own
// handler sets cancelled = true when the actor is Shocked (attack-type
// actions no-op for zero cost while Shocked; movement still works).
struct BeforeAttackEvent
{
    Vec2 direction;
    bool has_weapon = false;
    WeaponRangeShape range_shape = WeaponRangeShape::SingleTarget;
    int range = 0;
    int hits_per_turn = 0;
    std::vector<RaceBonusEntry> race_bonuses;
    StatsComponent attacker_stats;
    Element element = Element::None;    // the equipped weapon's own elemental flavor, 0/None = non-elemental
    std::uint32_t status_effect_id = 0; // weapon's on-hit ailment (NameId into StatusEffectLibrary), 0 = none
    int status_chance_percent = 0;      // chance per hit to apply status_effect_id, when element != None
    bool cancelled = false;
};

struct AfterAttackEvent
{
    bool found_target = false;
};

} // namespace psr
