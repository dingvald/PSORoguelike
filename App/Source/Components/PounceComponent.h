#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Data for AiBehavior::KeepDistanceAndPounce (see EnemyAiSystem.h) -- an
// entity with this component tries to hold itself at preferred_distance
// tiles (Chebyshev) from its target, retreating if the target closes past
// that and approaching if the target opens further than that. At exactly
// preferred_distance and 8-directionally aligned, it rolls
// pounce_chance_percent each turn to either lunge (see LungeAttackAction,
// whose own kLungeRange must match preferred_distance for the pounce to ever
// trigger) or strafe laterally instead. ghost_effect_prefab_id/duration
// author which VisualEffectSystem prefab LungeAttackAction spawns at the
// tile it jumps from (0 = no ghost, same "unregistered id is a safe no-op"
// convention as OnHitEffectComponent::effect_prefab_id).
struct PounceComponent
{
    int preferred_distance = 2;
    int pounce_chance_percent = 50;
    std::uint32_t ghost_effect_prefab_id = 0;
    float ghost_effect_duration = 0.25f;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<PounceComponent>("pounce")
            .Data<&PounceComponent::preferred_distance>("preferred_distance")
            .Data<&PounceComponent::pounce_chance_percent>("pounce_chance_percent")
            .Data<&PounceComponent::ghost_effect_prefab_id>("ghost_effect_prefab_id")
            .Data<&PounceComponent::ghost_effect_duration>("ghost_effect_duration");
    }
};

} // namespace psr
