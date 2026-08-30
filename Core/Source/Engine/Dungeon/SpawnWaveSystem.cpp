#include "Engine/Dungeon/SpawnWaveSystem.h"

#include "Engine/ECS/Position.h"
#include "Engine/ECS/SpawnWaveComponent.h"

#include <utility>

namespace psr {

SpawnWaveSystem::SpawnWaveSystem(Registry& registry, Grid& grid,
                                  std::unordered_map<std::uint32_t, int> initial_wave_counts,
                                  std::vector<PendingSpawnWave> pending_waves)
    : m_registry(&registry), m_grid(&grid), m_remaining_in_wave(std::move(initial_wave_counts))
{
    for (PendingSpawnWave& wave : pending_waves)
        m_queued_by_group[wave.group_id].push_back(std::move(wave));

    registry.OnDestroy<SpawnWaveComponent, &SpawnWaveSystem::OnSpawnWaveComponentDestroyed>(*this);
}

SpawnWaveSystem::~SpawnWaveSystem() { m_registry->DisconnectComponentLifecycle<SpawnWaveComponent>(*this); }

void SpawnWaveSystem::OnSpawnWaveComponentDestroyed(entt::registry& registry, entt::entity entity)
{
    const std::uint32_t group_id = registry.get<SpawnWaveComponent>(entity).group_id;

    auto it = m_remaining_in_wave.find(group_id);
    if (it == m_remaining_in_wave.end())
        return;
    if (--it->second > 0)
        return;

    m_remaining_in_wave.erase(it);
    SpawnNextWave(group_id);
}

void SpawnWaveSystem::SpawnNextWave(std::uint32_t group_id)
{
    auto queue_it = m_queued_by_group.find(group_id);
    if (queue_it == m_queued_by_group.end() || queue_it->second.empty())
        return;

    PendingSpawnWave wave = std::move(queue_it->second.front());
    queue_it->second.erase(queue_it->second.begin());

    int spawned_count = 0;
    for (const PendingSpawnEntry& entry : wave.entries)
    {
        if (entry.prefab_id == 0 || !m_registry->HasPrefab(entry.prefab_id))
            continue;
        const entt::entity entity = m_registry->CreateEntity(entry.prefab_id);
        m_registry->Emplace<Position>(entity, Position{entry.world_cell});
        m_grid->AddEntity(entry.world_cell, entity);
        m_registry->Emplace<SpawnWaveComponent>(entity, SpawnWaveComponent{group_id, wave.wave});
        ++spawned_count;
    }

    if (spawned_count > 0)
        m_remaining_in_wave[group_id] = spawned_count;
}

} // namespace psr
