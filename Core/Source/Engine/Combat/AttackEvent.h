#pragma once

#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h" // WeaponRangeShape, RaceBonusEntry
#include "Engine/Math/Vec2.h"

#include <vector>

namespace psr {

// Dispatched by AttackAction to the actor's own EventHandlerComponent
// (Entity::Dispatch) at the very start of Perform(), before AttackAction
// touches any component itself. EquipmentComponent's own AttachHandlers-
// registered handler resolves the equipped weapon (if any) and fills
// has_weapon/range_shape/range/hits_per_turn/race_bonuses/attacker_stats --
// AttackAction never reads EquipmentComponent/WeaponComponent directly.
struct BeforeAttackEvent
{
    Vec2 direction;
    bool has_weapon = false;
    WeaponRangeShape range_shape = WeaponRangeShape::SingleTarget;
    int range = 0;
    int hits_per_turn = 0;
    std::vector<RaceBonusEntry> race_bonuses;
    StatsComponent attacker_stats;
};

struct AfterAttackEvent
{
    bool found_target = false;
};

} // namespace psr
