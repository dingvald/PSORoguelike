#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Data for AiBehavior::PackFollower (see EnemyAiSystem.h) -- otherwise
// behaves exactly like ChaseAndAttack, but each turn also checks whether any
// living RaceComponent::race_id == pack_leader_race_id entity is within the
// AiComponent's own detection_range. The first turn none is found (the pack
// leader died, or wandered out of range and never returns), this entity
// takes a one-time permanent ATP/DFP penalty to its own StatsComponent and
// stops checking -- a simplified stand-in for PSO's Barbarous-Wolf-death
// panic howl (real PSO applies a temporary Jellen/Zalure debuff via an
// authored pack *group*; this engine has no per-instance pack-membership
// authoring yet, so membership is approximated purely at query time by race).
struct PackFollowerComponent
{
    std::uint32_t pack_leader_race_id = 0;

    // Runtime -- deliberately not registered below, so a panic is applied at
    // most once per entity regardless of how many further turns it takes
    // without a leader nearby.
    bool has_panicked = false;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<PackFollowerComponent>("pack_follower")
            .Data<&PackFollowerComponent::pack_leader_race_id>("pack_leader_race_id");
    }
};

} // namespace psr
