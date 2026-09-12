#include "Engine/Dungeon/DungeonInstantiator.h"

#include "Engine/Dungeon/DoorComponent.h"
#include "Engine/Dungeon/SwitchComponent.h"
#include "Engine/ECS/Position.h"

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
                                        Registry& registry, Grid& grid)
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

    // Doors: layout.locks first, then every remaining connected socket
    // bordering a Room/Vault/BossArena piece gets an always-open door.
    // Each door is stamped on whichever side of the connection is door-
    // bearing (piece-authored "door" tag on Room/Vault/BossArena sockets,
    // vs. "hallway" on Corridor ones -- see PieceSocket's own doc comment)
    // rather than always cell_a, since cell_a is just whichever piece
    // DungeonStitcher happened to place first while growing the tree and is
    // as likely to be the Corridor side as the room side.
    // handled_edges identifies a connection by its (cell_a, cell_b) pair --
    // not by the single cell the door ends up stamped on, since that cell
    // depends on which side is door-bearing and so isn't a stable key by
    // itself. Small (bounded by the dungeon's own lock count) -- a linear
    // scan is fine here, same reasoning FindDeadEnd's own doc comment gives
    // for dead-end lookups below.
    const auto is_door_bearing = [](PieceCategory category) { return category != PieceCategory::Corridor; };
    const auto door_cell_of = [&](const SocketConnection& connection, std::size_t door_side_piece_index)
    { return door_side_piece_index == connection.piece_a ? connection.cell_a : connection.cell_b; };
    const auto same_edge = [](const SocketConnection& a, const SocketConnection& b)
    { return a.cell_a == b.cell_a && a.cell_b == b.cell_b; };

    std::vector<SocketConnection> handled_edges;
    for (const LockAnnotation& lock : layout.locks)
    {
        const Vec2 door_local_cell = door_cell_of(lock.edge, lock.inside_room_index);
        const entt::entity door_entity = stamp(door_local_cell + offset, layout.locked_door_prefab_id);
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
        handled_edges.push_back(lock.edge);
    }

    for (const SocketConnection& connection : layout.connections)
    {
        if (std::any_of(handled_edges.begin(), handled_edges.end(),
                        [&](const SocketConnection& edge) { return same_edge(edge, connection); }))
            continue;

        const DungeonPiece* piece_a = library.Find(layout.pieces[connection.piece_a].piece_id);
        const DungeonPiece* piece_b = library.Find(layout.pieces[connection.piece_b].piece_id);
        const bool a_bears_door = piece_a && is_door_bearing(piece_a->category);
        const bool b_bears_door = piece_b && is_door_bearing(piece_b->category);
        if (!a_bears_door && !b_bears_door)
            continue;

        const Vec2 door_local_cell = a_bears_door ? connection.cell_a : connection.cell_b;
        const entt::entity door_entity = stamp(door_local_cell + offset, layout.unlocked_door_prefab_id);
        if (door_entity == entt::null)
            continue;
        // Both sides can be non-Corridor (e.g. a room-to-room connection) --
        // registered under both group ids so whichever room the player steps
        // into first locks it; the other side's LockRoomOnEntry call then
        // just finds it already invalid (see Registry::IsValid guard there).
        if (a_bears_door)
            result.room_entry_doors[static_cast<std::uint32_t>(connection.piece_a)].push_back(door_entity);
        if (b_bears_door)
            result.room_entry_doors[static_cast<std::uint32_t>(connection.piece_b)].push_back(door_entity);
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
        for (auto& [wave_number, entries] : waves_by_number)
            result.pending_spawn_waves.push_back(PendingSpawnWave{group_id, wave_number, std::move(entries)});
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
