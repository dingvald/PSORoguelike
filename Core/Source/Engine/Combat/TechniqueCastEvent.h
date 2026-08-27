#pragma once

#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h" // RaceBonusEntry

#include <cstdint>
#include <vector>

namespace psr {

// Dispatched by TechniqueAction to the caster's own EventHandlerComponent
// (Entity::Dispatch) at the very start of Perform(), before TechniqueAction
// touches any component itself. EquipmentComponent's own handler resolves
// the equipped weapon (if any) and fills has_weapon/weapon_grants_id/
// race_bonuses/attacker_stats; TPComponent's own handler fills current_tp/
// has_tp_component. TechniqueAction checks these fields in place of reading
// EquipmentComponent/WeaponComponent/TPComponent directly, then still
// performs the actual TP deduction itself once past the gate -- see
// AfterTechniqueCastEvent, dispatched right after, once the cast is
// confirmed to happen.
struct BeforeTechniqueCastEvent
{
    std::uint32_t technique_id = 0;
    bool has_weapon = false;
    bool weapon_grants_id = false;
    int current_tp = 0;
    bool has_tp_component = false;
    std::vector<RaceBonusEntry> race_bonuses;
    StatsComponent attacker_stats;
};

struct AfterTechniqueCastEvent
{
    std::uint32_t technique_id = 0;
};

} // namespace psr
