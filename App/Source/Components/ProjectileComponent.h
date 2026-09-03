#pragma once

#include "Combat/EffectFamily.h"
#include "Combat/Element.h"
#include "Components/StatsComponent.h"
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
// AttackAction's own lunge-then-resolve pattern.
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
};

} // namespace psr
