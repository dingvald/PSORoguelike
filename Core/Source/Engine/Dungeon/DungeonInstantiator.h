#pragma once

#include "Engine/Dungeon/DungeonStitcher.h"
#include "Engine/Dungeon/PendingSpawnWave.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Dungeon/RoomMap.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Rect.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace psr {

// The bounding rect, in a DungeonLayout's own (possibly-negative) world
// coordinates, covering every placed piece's cells -- DungeonStitcher always
// places the Entrance at world_offset{0,0}, but growth can extend the tree
// in any of the four directions from there, so a layout's coordinates aren't
// guaranteed non-negative. Callers use this to size a Grid (Grid is
// zero-based only) and compute InstantiateDungeon's offset before calling
// it: Rect bounds = ComputeDungeonBounds(layout, library); Grid grid(bounds.size.x,
// bounds.size.y); InstantiateDungeon(layout, library, -bounds.origin, registry, grid);
// An empty layout (no resolvable pieces) returns a zero-sized Rect at the origin.
Rect ComputeDungeonBounds(const DungeonLayout& layout, const PieceLibrary& library);

// Where a spawning system (e.g. a gameplay layer placing the player) should
// enter the instantiated dungeon -- the Entrance piece's first authored
// cell, translated the same way every other cell was. DungeonStitcher always
// places the Entrance as layout.pieces[0], before growth begins.
struct DungeonInstantiation
{
    Vec2 entrance_tile;

    // Tags every stamped tile with the placed-piece index (into
    // layout.pieces) it came from -- the same index SpawnWaveComponent::
    // group_id below uses -- so App-level systems (see
    // RoomVisibilityTracker/FogOfWarRenderableLookup) can tell which room a
    // tile belongs to after generation's own DungeonLayout is gone.
    RoomMap room_map;

    // room_adjacency[i] lists every room index sharing a SocketConnection
    // (layout.connections) with room i, both directions -- keyed the same
    // way room_map is (placed-piece index). Lets App-level fog of war (see
    // RoomVisibilityTracker) extend visibility from the player's current
    // room to the rooms beyond its doorways.
    std::vector<std::vector<std::uint32_t>> room_adjacency;

    // group_id (a placed piece's index into layout.pieces) -> how many
    // entities were stamped for that group's first (lowest-numbered) wave,
    // immediately during instantiation. Only present for groups that
    // authored at least one PieceSpawn.
    std::unordered_map<std::uint32_t, int> initial_wave_counts;

    // Every wave after each group's first, in ascending (group, wave) order,
    // not yet spawned -- SpawnWaveSystem consumes these as earlier waves die.
    std::vector<PendingSpawnWave> pending_spawn_waves;
};

// Stamps every placed piece's cells into grid as live entities: each cell's
// PieceCellPrefab list is instantiated via registry.CreateEntity(prefab_id)
// (skipped if that id isn't a registered prefab, or is 0) and added to grid
// at world_offset + offset + cell.offset -- offset is normally
// -ComputeDungeonBounds(...).origin, translating the layout's own coordinate
// space into grid's zero-based one -- with a Position stamped to match.
// Separately, each placed piece's own PieceSocket list is walked: a socket
// layout.dead_ends marks as unconnected has its fallback_prefab_id stamped
// into its cell the same way (skipped if fallback_prefab_id is 0) -- a
// connected socket stamps nothing of its own, since it carries no visual,
// only the two pieces' ordinary cell prefabs meeting at the border. grid
// must already be sized to fit (see ComputeDungeonBounds) -- this function
// does not resize it. A piece_id from layout.pieces that library can't
// resolve is skipped (defensive: GenerateDungeon only ever placed pieces it
// resolved from this same library, so this only matters if a stale/
// mismatched library is passed).
//
// Each placed piece's own PieceSpawn list is grouped by wave number: the
// lowest-numbered wave stamps immediately (tagged with SpawnWaveComponent so
// SpawnWaveSystem can track it), and every later wave is returned via
// DungeonInstantiation::pending_spawn_waves for SpawnWaveSystem to spawn once
// the previous wave's entities all die.
//
// on_spawned, if set, is invoked once for each PieceSpawn-sourced entity
// stamped here (first-wave only -- later waves go through SpawnWaveSystem's
// own on_spawned instead), right after its SpawnWaveComponent is emplaced.
// Not called for plain cell/dead-end-socket prefabs (static dungeon
// furniture, not creatures). This is Core's only hook for App-level, per-
// creature setup (e.g. joining the turn queue) that Core itself can't
// perform, since the components involved (ActorComponent, EquipmentComponent)
// are App-level -- see GameplayLayer::OnAttach.
DungeonInstantiation InstantiateDungeon(const DungeonLayout& layout, const PieceLibrary& library, Vec2 offset,
                                        Registry& registry, Grid& grid,
                                        std::function<void(entt::entity)> on_spawned = {});

} // namespace psr
