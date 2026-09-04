#pragma once

#include <entt/entt.hpp>

namespace psr {

// Tags an entity created by AiBehavior::StationarySpawner with the spawning
// entity's own handle, so that spawner can count how many of its own
// spawns (e.g. a Monest's Mothmants) are still alive against
// SpawnerAiComponent::max_alive. Deliberately not meta-registered -- purely
// runtime state, never hand-authored in a prefab, same precedent as
// EquipmentComponent/TweenComponent.
struct SpawnedByComponent
{
    entt::entity owner = entt::null;
};

} // namespace psr
