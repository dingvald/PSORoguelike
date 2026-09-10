#pragma once

#include "Combat/EffectFamily.h"
#include "Combat/Element.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h" // RaceBonusEntry
#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <entt/entt.hpp>
#include <vector>

namespace psr {

// Pure runtime state for an in-flight technique projectile -- spawned by
// TechniqueAction, advanced one hop per turn by ProjectileAdvanceAction (see
// its own doc comment), never authored in prefab JSON, so deliberately not
// schema/meta-registered (same treatment as TweenComponent).
//
// path is the full tile sequence resolved at cast time (via
// TargetResolution.h's BuildProjectilePath) and never mutated afterward;
// next_hop is the only thing that advances. Everything else here is a
// snapshot of the casting technique/caster taken at spawn time -- the actual
// hit roll (hit chance, crit, variance) happens fresh at impact, against
// whatever occupies the tile the projectile stops on, same as
// WeaponAttackAction's own lunge-then-resolve pattern.
struct ProjectileComponent
{
    std::vector<Vec2> path;
    std::size_t next_hop = 0;
    int step_cost = 20; // energy this action costs per hop -- see TechniqueAction's own spawn-time computation

    entt::entity source = entt::null; // original caster -- damage/events attribute to THIS entity, not the projectile
    bool pierces = false;             // false: resolve only path.back(); true: resolve every tile in path

    StatsComponent attacker_stats;
    float power_multiplier = 1.0f;
    EffectFamily effect_family = EffectFamily::Damage;
    Element element = Element::None; // resolved against the target's ElementalResistanceComponent at impact
    std::uint32_t status_effect_id = 0;
    int status_chance_percent = 0;

    std::uint32_t hit_effect_prefab_id = 0;
    float hit_effect_duration = 0.3f;

    // true: a weapon's ranged attack (WeaponAttackAction) -- ResolveProjectileImpact
    // computes physical (ATP-vs-DFP, race-bonus, crit) damage via
    // attacker_stats/race_bonuses instead of the default Technique-style
    // MST-based magic formula. false (default): unchanged Technique
    // projectile behavior.
    bool physical_damage = false;
    std::vector<RaceBonusEntry> race_bonuses; // only meaningful when physical_damage

    // Extra energy debited from the target's TurnQueue schedule on a landed
    // hit -- see WeaponComponent::hit_stun_energy's own doc comment for the
    // units. 0 (default) for every existing Technique projectile. Never
    // paired with knockback -- a ranged hit stuns but never pushes (see
    // ResolveProjectileImpact.cpp).
    int hit_stun_energy = 0;
};

} // namespace psr
