#include "Systems/TabTargetSystem.h"

#include "Combat/Hostility.h"
#include "Components/TabTargetComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace psr {

namespace {
    constexpr const char* kMarkerPrefabId = "ui.tab_target_marker";

    // Every hostile HealthComponent entity (excluding player) with a
    // Position in a RoomVisibility::Visible room, sorted nearest-first from
    // player's own tile. Mirrors EnemyAiSystem's FindNearestHostileTile
    // shape, but collects every candidate rather than tracking only the
    // single best.
    std::vector<entt::entity> SortedHostilesByDistance(Registry& registry, Entity player, const RoomMap& room_map,
                                                        const RoomVisibilityTracker& visibility)
    {
        const Vec2 origin = player.Get<Position>().tile;
        std::vector<std::pair<int, entt::entity>> candidates;
        registry.Each<HealthComponent>(
            [&](entt::entity candidate, HealthComponent&)
            {
                Entity target(registry, candidate);
                if (!IsHostile(player, target))
                    return;
                const Position* position = target.TryGet<Position>();
                if (!position)
                    return;
                if (visibility.GetVisibility(room_map.GetRoom(position->tile)) != RoomVisibility::Visible)
                    return;
                candidates.emplace_back(ManhattanDistance(origin, position->tile), candidate);
            });
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });

        std::vector<entt::entity> sorted;
        sorted.reserve(candidates.size());
        for (const auto& [distance, entity] : candidates)
            sorted.push_back(entity);
        return sorted;
    }
} // namespace

TabTargetSystem::TabTargetSystem(Registry& registry, Grid& grid, const RoomMap& room_map,
                                 const RoomVisibilityTracker& visibility)
    : m_registry(&registry), m_grid(&grid), m_room_map(&room_map), m_visibility(&visibility)
{
}

bool TabTargetSystem::IsVisible(entt::entity target) const
{
    const Position* position = m_registry->TryGetComponent<Position>(target);
    if (!position)
        return false;
    return m_visibility->GetVisibility(m_room_map->GetRoom(position->tile)) == RoomVisibility::Visible;
}

void TabTargetSystem::CycleTarget(Entity player)
{
    const std::vector<entt::entity> sorted = SortedHostilesByDistance(*m_registry, player, *m_room_map, *m_visibility);
    if (sorted.empty())
    {
        ClearTarget(player);
        return;
    }

    TabTargetComponent& tab_target = player.GetOrEmplace<TabTargetComponent>();
    const auto it = std::find(sorted.begin(), sorted.end(), tab_target.target);
    std::size_t next_index = 0;
    if (it != sorted.end())
    {
        next_index = static_cast<std::size_t>(it - sorted.begin()) + 1;
        if (next_index >= sorted.size())
            next_index = 0;
    }

    tab_target.target = sorted[next_index];
    RepositionMarker(m_registry->GetComponent<Position>(tab_target.target).tile);
}

void TabTargetSystem::ClearTarget(Entity player)
{
    player.GetOrEmplace<TabTargetComponent>().target = entt::null;
    RemoveMarker();
}

void TabTargetSystem::Update(Entity player)
{
    TabTargetComponent* tab_target = player.TryGet<TabTargetComponent>();
    if (!tab_target || tab_target->target == entt::null)
        return;

    if (!m_registry->IsValid(tab_target->target) || !IsVisible(tab_target->target))
    {
        tab_target->target = entt::null;
        RemoveMarker();
        return;
    }

    const Vec2 target_tile = m_registry->GetComponent<Position>(tab_target->target).tile;
    if (m_marker_entity == entt::null || target_tile != m_marker_tile)
        RepositionMarker(target_tile);
}

void TabTargetSystem::RepositionMarker(Vec2 tile)
{
    if (m_marker_entity == entt::null)
    {
        m_marker_entity = m_registry->CreateEntity(entt::hashed_string::value(kMarkerPrefabId));
        m_grid->AddEntity(tile, m_marker_entity);
    }
    else
    {
        m_grid->RemoveEntity(m_marker_tile, m_marker_entity);
        m_grid->AddEntity(tile, m_marker_entity);
    }
    m_marker_tile = tile;
}

void TabTargetSystem::RemoveMarker()
{
    if (m_marker_entity == entt::null)
        return;
    m_grid->RemoveEntity(m_marker_tile, m_marker_entity);
    m_registry->DestroyEntity(m_marker_entity);
    m_marker_entity = entt::null;
}

} // namespace psr
