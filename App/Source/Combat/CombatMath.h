#pragma once

#include "Components/WeaponComponent.h"

#include <cstdint>
#include <vector>

namespace psr {

// Pure combat-formula helpers, shaped after PSO (GameCube, Episode I&II)'s
// community-reverse-engineered ATA-vs-EVP hit chance and ATP-vs-DFP damage
// mechanics -- PSO never published official formulas, so these constants
// come from long-standing community research (Ephinea wiki, PSO-World),
// cross-checked but not primary-source-verified. Real per-entity numbers are
// authored content (StatsComponent), same deferral every other milestone
// here already makes; only the formula *shape* is fixed by this file.

// Accuracy = attacker_ata - defender_evp * 0.2, clamped to [0, 100] and
// expressed as a fraction. Unlike a simple ratio, this allows genuinely
// guaranteed hits (Accuracy >= 100) and guaranteed misses (Accuracy <= 0),
// matching PSO's real hit-chance behavior.
float ComputeHitChance(int attacker_ata, int defender_evp);

// PSO's per-swing combo ramp: hit_index 0/1/2/... -> 1.0 / 1.3 / 1.69 / ...
// (1.3^hit_index). Scales ATA (not damage) for successive hits within one
// attack's hits_per_turn loop.
float ComboAtaMultiplier(int hit_index);

// max(1, floor(((attacker_atp - defender_dfp) / 5) * 0.9 * variance_roll)).
// variance_roll is expected in [0.9, 1.1], standing in for PSO's per-weapon
// min/max damage roll (this codebase has no per-weapon min/max ATP fields to
// compute the real spread from).
int ComputeDamage(int attacker_atp, int defender_dfp, float variance_roll);

// Technique damage: max(1, floor((attacker_mst / 5) * (1 - resistance_percent
// / 100))). PSO techniques bypass DFP entirely (unlike physical damage) and
// have no random variance -- only the target's elemental resistance for the
// spell's element reduces them.
int ComputeTechniqueDamage(int attacker_mst, int resistance_percent);

// The bonus elemental damage an elemental weapon (WeaponComponent::element !=
// None) adds on top of ComputeDamage's physical result -- PSO's own "an
// elemental hit deals its physical damage plus a smaller elemental kicker"
// shape, distinct from a Technique's all-elemental, DFP-bypassing damage.
// max(0, floor((attacker_atp / 10) * (1 - resistance_percent / 100) *
// (is_special_attack ? 2.0 : 1.0))): half a Technique's per-ATP/MST rate
// normally (a passive elemental proc shouldn't out-damage the weapon's own
// swing), doubled when triggered as the weapon's own Special Attack (see
// WeaponAttackAction) -- matching that action's guaranteed (rather than
// chance-rolled) status application. Floored at 0, not 1: a fully-resisted
// elemental hit should add nothing, unlike a Technique (which is *entirely*
// elemental and must still land for *something*).
int ComputeElementalDamage(int attacker_atp, int resistance_percent, bool is_special_attack);

// resistance_percent's mitigation applied to an elemental status effect's own
// proc chance (not its damage): max(0, round(chance_percent * (1 -
// resistance_percent / 100))). Shared by every elemental attack path
// (WeaponAttackAction/ProjectileImpact/PhotonArtAction) right before calling
// MaybeApplyElementalStatus, so a resistant target shrugs off status
// ailments proportionally to its resistance, same as it already takes less
// elemental damage.
int ApplyResistanceToStatusChance(int chance_percent, int resistance_percent);

// PSO's LCK-driven critical hit chance, expressed as a fraction: lck / 500
// (LCK / 5%), clamped to [0, 1].
float ComputeCritChance(int attacker_lck);

// is_critical ? round(damage * 1.5) : damage.
int ApplyCritical(int damage, bool is_critical);

// Multiplies value by (1 + bonus_percent / 100) for the race_bonuses entry
// matching defender_race_id, if any; unchanged otherwise. Callers scale the
// attacker's ATP with this before ComputeDamage (PSO's Native/A.Beast/
// Machine/Dark weapon attribute), not the final damage.
int ApplyRaceBonus(int value, const std::vector<RaceBonusEntry>& race_bonuses, std::uint32_t defender_race_id);

} // namespace psr
