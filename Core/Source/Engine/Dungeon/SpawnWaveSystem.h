#pragma once

#include "Engine/Dungeon/PendingSpawnWave.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace psr {

// Owns spawning every authored PieceSpawn wave for every group (a piece
// placement): DungeonInstantiator hands every wave to this system as a
// PendingSpawnWave, unspawned. A group's earliest wave spawns once the
// player first enters that room (see TriggerRoomEntered, driven by
// GameplayLayer's room-change detection); every later wave spawns once every
// entity tagged with the group's current wave has died.
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
    // (every wave, first and later alike) -- Core's only hook for App-level,
    // per-creature setup (e.g. joining the turn queue) that Core itself
    // can't perform.
    SpawnWaveSystem(Registry& registry, Grid& grid, std::vector<PendingSpawnWave> pending_waves,
                    std::function<void(entt::entity)> on_spawned = {});
    ~SpawnWaveSystem();

    // Bound on_destroy<SpawnWaveComponent> listener captures this instance's
    // address -- neither copying nor moving would keep it valid (C.21/C.81).
    SpawnWaveSystem(const SpawnWaveSystem&) = delete;
    SpawnWaveSystem& operator=(const SpawnWaveSystem&) = delete;
    SpawnWaveSystem(SpawnWaveSystem&&) = delete;
    SpawnWaveSystem& operator=(SpawnWaveSystem&&) = delete;

    // Spawns group_id's earliest still-queued wave, unless one is already in
    // flight for it (SpawnNextWave already handles an empty/absent queue
    // gracefully, so this is a harmless no-op for a room with no spawns, or
    // one already fully cleared). Called once per group the first time the
    // player enters that room -- see GameplayLayer::EnterRoom. Safe to call
    // again on re-entry: the guard below only lets a group's very next
    // not-yet-spawned wave through, whether that's "never started" or
    // "cleared, nothing left queued".
    void TriggerRoomEntered(std::uint32_t group_id);

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
