#include "Engine/Dungeon/DungeonInstantiator.h"

#include "Engine/Dungeon/DoorComponent.h"
#include "Engine/Dungeon/SwitchComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SpawnWaveComponent.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>

namespace psr {

Rect ComputeDungeonBounds(const DungeonLayout& layout, const PieceLibrary& library)
{
    bool any = false;
    Vec2 min{};
    Vec2 max{}; // inclusive

    for (const PlacedPiece& placed : layout.pieces)
    {
        const DungeonPiece* piece = library.Find(placed.piece_id);
        if (!piece)
            continue;

        for (const PieceCell& cell : piece->cells)
        {
            const Vec2 world_cell = placed.world_offset + ApplyPieceTransform(cell.offset, placed.transform);
            if (!any)
            {
                min = max = world_cell;
                any = true;
                continue;
            }
            min.x = std::min(min.x, world_cell.x);
            min.y = std::min(min.y, world_cell.y);
            max.x = std::max(max.x, world_cell.x);
            max.y = std::max(max.y, world_cell.y);
        }
    }

    if (!any)
        return Rect{{0, 0}, {0, 0}};
    return Rect{min, {max.x - min.x + 1, max.y - min.y + 1}};
}

namespace {

    // dead_ends is small (bounded by a dungeon's own socket count) -- a linear
    // scan per socket cell is simpler and safer here than packing
    // (piece_index, world_cell, edge) into a hashable key, and this only ever
    // runs once per generated dungeon, not per frame.
    const DeadEndSocket* FindDeadEnd(const DungeonLayout& layout, std::size_t piece_index, Vec2 world_cell,
                                     EdgeDirection edge)
    {
        for (const DeadEndSocket& dead_end : layout.dead_ends)
            if (dead_end.piece_index == piece_index && dead_end.world_cell == world_cell && dead_end.edge == edge)
                return &dead_end;
        return nullptr;
    }

} // namespace

DungeonInstantiation InstantiateDungeon(const DungeonLayout& layout, const PieceLibrary& library, Vec2 offset,
                                        Registry& registry, Grid& grid, std::function<void(entt::entity)> on_spawned)
{
    const auto stamp = [&](Vec2 grid_cell, std::uint32_t prefab_id) -> entt::entity
    {
        if (prefab_id == 0 || !registry.HasPrefab(prefab_id))
            return entt::null;
        const entt::entity entity = registry.CreateEntity(prefab_id);
        registry.Emplace<Position>(entity, Position{grid_cell});
        grid.AddEntity(grid_cell, entity);
        return entity;
    };

    DungeonInstantiation result;
    result.room_map = RoomMap(grid.GetWidth(), grid.GetHeight());

    result.room_adjacency.resize(layout.pieces.size());
    for (const SocketConnection& connection : layout.connections)
    {
        if (connection.piece_a < result.room_adjacency.size())
            result.room_adjacency[connection.piece_a].push_back(static_cast<std::uint32_t>(connection.piece_b));
        if (connection.piece_b < result.room_adjacency.size())
            result.room_adjacency[connection.piece_b].push_back(static_cast<std::uint32_t>(connection.piece_a));
    }

    // Doors: layout.locks first (each claims its own edge's cell_a), then
    // every remaining connected socket bordering a Room/Vault/BossArena piece
    // gets an always-open door. handled_door_cells is small (bounded by the
    // dungeon's own lock count) -- a linear scan is fine here, same reasoning
    // FindDeadEnd's own doc comment gives for dead-end lookups below.
    std::vector<Vec2> handled_door_cells;
    for (const LockAnnotation& lock : layout.locks)
    {
        const Vec2 door_cell = lock.edge.cell_a + offset;
        const entt::entity door_entity = stamp(door_cell, layout.locked_door_prefab_id);
        if (door_entity == entt::null)
            continue;

        const auto group_id = static_cast<std::uint32_t>(lock.inside_room_index);
        registry.Emplace<DoorComponent>(
            door_entity, DoorComponent{lock.unlock_condition, layout.unlocked_door_prefab_id, group_id,
                                       lock.unlock_condition == DoorUnlockCondition::Switch ? 1 : 0});
        if (lock.unlock_condition == DoorUnlockCondition::RoomCleared)
            result.room_cleared_doors[group_id].push_back(door_entity);
        else
        {
            const entt::entity switch_entity = stamp(lock.switch_cell + offset, layout.switch_prefab_id);
            if (switch_entity != entt::null)
                registry.Emplace<SwitchComponent>(switch_entity, SwitchComponent{door_entity, false});
        }
        handled_door_cells.push_back(lock.edge.cell_a);
    }

    const auto is_room_like = [](PieceCategory category)
    { return category == PieceCategory::Room || category == PieceCategory::Vault ||
             category == PieceCategory::BossArena; };
    for (const SocketConnection& connection : layout.connections)
    {
        if (std::find(handled_door_cells.begin(), handled_door_cells.end(), connection.cell_a) !=
            handled_door_cells.end())
            continue;

        const DungeonPiece* piece_a = library.Find(layout.pieces[connection.piece_a].piece_id);
        const DungeonPiece* piece_b = library.Find(layout.pieces[connection.piece_b].piece_id);
        const bool borders_room = (piece_a && is_room_like(piece_a->category)) ||
                                  (piece_b && is_room_like(piece_b->category));
        if (borders_room)
            stamp(connection.cell_a + offset, layout.unlocked_door_prefab_id);
    }

    for (std::size_t piece_index = 0; piece_index < layout.pieces.size(); ++piece_index)
    {
        const PlacedPiece& placed = layout.pieces[piece_index];
        const DungeonPiece* piece = library.Find(placed.piece_id);
        if (!piece)
            continue;

        for (const PieceCell& cell : piece->cells)
        {
            const Vec2 grid_cell = placed.world_offset + ApplyPieceTransform(cell.offset, placed.transform) + offset;
            result.room_map.SetRoom(grid_cell, static_cast<std::uint32_t>(piece_index));
            for (const PieceCellPrefab& prefab : cell.prefabs)
                stamp(grid_cell, prefab.prefab_id);
        }

        for (const PieceSocket& socket : piece->sockets)
        {
            const Vec2 world_cell = placed.world_offset + ApplyPieceTransform(socket.cell_offset, placed.transform);
            const EdgeDirection world_edge = ApplyPieceTransform(socket.edge, placed.transform);
            if (const DeadEndSocket* dead_end = FindDeadEnd(layout, piece_index, world_cell, world_edge))
                stamp(world_cell + offset, dead_end->fallback_prefab_id);
        }

        if (piece->spawns.empty())
            continue;

        std::map<int, std::vector<PendingSpawnEntry>> waves_by_number;
        for (const PieceSpawn& spawn : piece->spawns)
        {
            const Vec2 world_cell =
                placed.world_offset + ApplyPieceTransform(spawn.cell_offset, placed.transform) + offset;
            waves_by_number[spawn.wave].push_back(PendingSpawnEntry{world_cell, spawn.prefab_id});
        }

        const std::uint32_t group_id = static_cast<std::uint32_t>(piece_index);
        bool is_first_wave = true;
        for (auto& [wave_number, entries] : waves_by_number)
        {
            if (is_first_wave)
            {
                int spawned_count = 0;
                for (const PendingSpawnEntry& entry : entries)
                {
                    const entt::entity entity = stamp(entry.world_cell, entry.prefab_id);
                    if (entity == entt::null)
                        continue;
                    registry.Emplace<SpawnWaveComponent>(entity, SpawnWaveComponent{group_id, wave_number});
                    if (on_spawned)
                        on_spawned(entity);
                    ++spawned_count;
                }
                if (spawned_count > 0)
                    result.initial_wave_counts[group_id] = spawned_count;
                is_first_wave = false;
            }
            else
            {
                result.pending_spawn_waves.push_back(PendingSpawnWave{group_id, wave_number, std::move(entries)});
            }
        }
    }

    result.entrance_tile = offset;
    if (!layout.pieces.empty())
    {
        const PlacedPiece& entrance = layout.pieces[0];
        if (const DungeonPiece* entrance_piece = library.Find(entrance.piece_id);
            entrance_piece && !entrance_piece->cells.empty())
            result.entrance_tile = entrance.world_offset +
                                   ApplyPieceTransform(entrance_piece->cells[0].offset, entrance.transform) + offset;
    }
    return result;
}

} // namespace psr
