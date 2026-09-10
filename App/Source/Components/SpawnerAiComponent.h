#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Data for AiBehavior::StationarySpawner (see EnemyAiSystem.h) -- an entity
// with this component never moves or attacks; each of its turns it counts
// down cooldown_remaining and, once it reaches zero, spawns one
// spawn_prefab_id entity into an adjacent open tile (skipped if max_alive
// entities it previously spawned, tracked via SpawnedByComponent, are still
// alive), then resets the countdown to cooldown_turns. Mirrors Monest
// releasing Mothmants in PSO.
struct SpawnerAiComponent
{
    std::uint32_t spawn_prefab_id = 0;
    int cooldown_turns = 5;
    int max_alive = 3;

    // Runtime countdown -- deliberately not registered below (no .Data<>()
    // call), so it's invisible to JSON authoring/round-trip: 0 means eligible
    // to spawn on this entity's very first turn.
    int cooldown_remaining = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<SpawnerAiComponent>("spawner_ai")
            .Data<&SpawnerAiComponent::spawn_prefab_id>("spawn_prefab_id")
            .Data<&SpawnerAiComponent::cooldown_turns>("cooldown_turns")
            .Data<&SpawnerAiComponent::max_alive>("max_alive");
    }
};

} // namespace psr
