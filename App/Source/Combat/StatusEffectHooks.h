#pragma once

#include "Combat/Element.h"
#include "Engine/ECS/Entity.h"

#include <cstdint>
#include <random>

namespace psr {

class StatusEffectLibrary;

// The shared "an elemental hit gets one roll that gates both its ailment and
// its own bonus damage" hook WeaponAttackAction/ProjectileImpact/
// PhotonArtAction each call once, right after a physical hit's base damage is
// computed but before it dispatches IncomingDamageEvent -- the returned bonus
// (0 on a miss, element == Element::None, or chance_percent <= 0) folds
// straight into that same damage number, so a proc reads as one hit rather
// than two separate pop-ins. On a successful roll, also applies
// status_effect_id directly (a no-op if it's 0 -- an elemental weapon
// authored with bonus damage but no ailment).
//
// resistance_percent (the target's ElementalResistanceComponent::ResistanceFor
// (element), or 0 if it has none -- callers fetch this themselves since they
// already need target's other components) mitigates both halves: it lowers
// chance_percent (see CombatMath's ApplyResistanceToStatusChance) before the
// roll, and lowers the bonus's own magnitude (see ComputeElementalDamage) once
// it lands. is_special_attack (a manually-triggered Special Attack hotbar
// slot, not a passive proc -- see WeaponAttackAction) forces the roll to 100%
// pre-resistance and doubles the resulting bonus via ComputeElementalDamage.
int RollElementalDamageBonus(Entity target, const StatusEffectLibrary& library, Element element,
                             std::uint32_t status_effect_id, int chance_percent, int resistance_percent,
                             int attacker_atp, bool is_special_attack, std::mt19937& rng);

} // namespace psr
