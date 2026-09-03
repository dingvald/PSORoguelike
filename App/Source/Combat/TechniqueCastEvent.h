#pragma once

#include "Components/StatsComponent.h"

#include <cstdint>

namespace psr {

// Dispatched by TechniqueAction to the caster's own EventHandlerComponent
// (Entity::Dispatch) at the very start of Perform(), before TechniqueAction
// touches any component itself. EquipmentComponent's own handler resolves
// the equipped weapon (if any) and fills has_weapon/weapon_grants_id/
// attacker_stats; TPComponent's own handler fills current_tp/
// has_tp_component. TechniqueAction checks these fields in place of reading
// EquipmentComponent/WeaponComponent/TPComponent directly, then still
// performs the actual TP deduction itself once past the gate -- see
// AfterTechniqueCastEvent, dispatched right after, once the cast is
// confirmed to happen. Unlike BeforeAttackEvent/BeforePhotonArtCastEvent,
// this event carries no element/status_effect_id/race_bonuses fields of its
// own -- Technique's element is spell-authored (Technique::element) rather
// than weapon-derived, and PSO's Native/A.Beast/Machine/Dark weapon
// attribute never modifies technique damage, so TechniqueAction has no use
// for either. StatusEffectComponent's own handler still sets cancelled =
// true when the caster is Shocked.
struct BeforeTechniqueCastEvent
{
    std::uint32_t technique_id = 0;
    bool has_weapon = false;
    bool weapon_grants_id = false;
    int current_tp = 0;
    bool has_tp_component = false;
    StatsComponent attacker_stats;
    bool cancelled = false;
};

struct AfterTechniqueCastEvent
{
    std::uint32_t technique_id = 0;
};

} // namespace psr
