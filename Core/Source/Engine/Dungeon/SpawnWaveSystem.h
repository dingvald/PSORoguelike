#pragma once

#include "Engine/Dungeon/PendingSpawnWave.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace psr {

// Gates enemy population past a piece's first authored spawn wave: the
// lowest-numbered wave of a piece's PieceSpawn list stamps immediately (see
// DungeonInstantiator), and every later wave is handed to this system as a
// PendingSpawnWave. It spawns the next wave for a group (a piece placement)
// once every entity tagged with that group's current wave has died.
//
// Deliberately has no dependency on DeathEvent/HealthSystem/DeathSystem: it
// reacts to SpawnWaveComponent being destroyed via entt's own on_destroy
// signal, which fires as a side effect of Registry::DestroyEntity regardless
// of why the entity died -- the same pattern TurnCoordinator already uses to
// react to ActorComponent destruction rather than DeathEvent.
class SpawnWaveSystem
{
public:
    // on_spawned, if set, is invoked once for each entity this system spawns
    // for a later wave -- same contract as DungeonInstantiator's own
    // on_spawned parameter (both should normally be given the same callback),
    // since a later-wave spawn needs the identical App-level, per-creature
    // setup (joining the turn queue, equipping an innate weapon) a first-wave
    // spawn gets there.
    SpawnWaveSystem(Registry& registry, Grid& grid, std::unordered_map<std::uint32_t, int> initial_wave_counts,
                    std::vector<PendingSpawnWave> pending_waves, std::function<void(entt::entity)> on_spawned = {});
    ~SpawnWaveSystem();

    // Bound on_destroy<SpawnWaveComponent> listener captures this instance's
    // address -- neither copying nor moving would keep it valid (C.21/C.81).
    SpawnWaveSystem(const SpawnWaveSystem&) = delete;
    SpawnWaveSystem& operator=(const SpawnWaveSystem&) = delete;
    SpawnWaveSystem(SpawnWaveSystem&&) = delete;
    SpawnWaveSystem& operator=(SpawnWaveSystem&&) = delete;

private:
    void OnSpawnWaveComponentDestroyed(entt::registry& registry, entt::entity entity);
    void SpawnNextWave(std::uint32_t group_id);

    Registry* m_registry;
    Grid* m_grid;
    std::unordered_map<std::uint32_t, int> m_remaining_in_wave;
    std::unordered_map<std::uint32_t, std::vector<PendingSpawnWave>> m_queued_by_group;
    std::function<void(entt::entity)> m_on_spawned;
};

} // namespace psr
