#pragma once

#include "Combat/Element.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h" // WeaponRangeShape, RaceBonusEntry
#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <vector>

namespace psr {

// Dispatched by WeaponAttackAction to the actor's own EventHandlerComponent
// (Entity::Dispatch) at the very start of Perform(), before WeaponAttackAction
// touches any component itself. EquipmentComponent's own AttachHandlers-
// registered handler resolves the equipped weapon (if any) and fills
// has_weapon/range_shape/range/hits_per_turn/race_bonuses/attacker_stats/
// element/status_effect_id/status_chance_percent/hit_effect_prefab_id/
// hit_effect_duration -- WeaponAttackAction never reads
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
    Element element = Element::None;        // the equipped weapon's own elemental flavor, 0/None = non-elemental
    std::uint32_t status_effect_id = 0;     // weapon's on-hit ailment (NameId into StatusEffectLibrary), 0 = none
    int status_chance_percent = 0;          // chance per hit to apply status_effect_id, when element != None
    std::uint32_t hit_effect_prefab_id = 0; // weapon's OnHitEffectComponent, 0 = none
    float hit_effect_duration = 0.3f;
    bool cancelled = false;

    // Mirror WeaponComponent's own fields of the same name -- see
    // WeaponAttackAction, which never reads WeaponComponent directly.
    bool fires_projectile = false;
    bool projectile_pierces = false;
    int projectile_speed = 5;
    std::uint32_t projectile_prefab_id = 0;
    int hit_stun_energy = 0;
};

struct AfterAttackEvent
{
    bool found_target = false;
};

} // namespace psr
