#include "Engine/Dungeon/DungeonInstantiator.h"

#include "Engine/ECS/Position.h"

#include <algorithm>
#include <cstddef>

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
            const Vec2 world_cell = placed.world_offset + cell.offset;
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
    const auto stamp = [&](Vec2 grid_cell, std::uint32_t prefab_id)
    {
        if (prefab_id == 0 || !registry.HasPrefab(prefab_id))
            return;
        const entt::entity entity = registry.CreateEntity(prefab_id);
        registry.Emplace<Position>(entity, Position{grid_cell});
        grid.AddEntity(grid_cell, entity);
    };

    for (std::size_t piece_index = 0; piece_index < layout.pieces.size(); ++piece_index)
    {
        const PlacedPiece& placed = layout.pieces[piece_index];
        const DungeonPiece* piece = library.Find(placed.piece_id);
        if (!piece)
            continue;

        for (const PieceCell& cell : piece->cells)
        {
            const Vec2 grid_cell = placed.world_offset + cell.offset + offset;
            for (const PieceCellPrefab& prefab : cell.prefabs)
                stamp(grid_cell, prefab.prefab_id);
        }

        for (const PieceSocket& socket : piece->sockets)
        {
            const Vec2 world_cell = placed.world_offset + socket.cell_offset;
            if (const DeadEndSocket* dead_end = FindDeadEnd(layout, piece_index, world_cell, socket.edge))
                stamp(world_cell + offset, dead_end->fallback_prefab_id);
        }
    }

    DungeonInstantiation result;
    result.entrance_tile = offset;
    if (!layout.pieces.empty())
    {
        const PlacedPiece& entrance = layout.pieces[0];
        if (const DungeonPiece* entrance_piece = library.Find(entrance.piece_id);
            entrance_piece && !entrance_piece->cells.empty())
            result.entrance_tile = entrance.world_offset + entrance_piece->cells[0].offset + offset;
    }
    return result;
}

} // namespace psr
