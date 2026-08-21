#pragma once

#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonPiece.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace psr {

// A socket prefab's authorable data (SocketComponent's fields), resolved by
// whatever mechanism the caller has on hand for querying a prefab's
// components -- see SocketLookup below. Kept separate from SocketComponent
// itself so this header (and the pure algorithm in DungeonStitcher.cpp)
// doesn't need to depend on the ECS/Registry machinery at all.
struct SocketInfo
{
    std::vector<std::string> tags;
    std::uint32_t fallback_prefab_id = 0;
};

// Resolves a prefab id to its SocketComponent data, or nullopt if that prefab
// isn't a socket. The stitcher is pure algorithm with no ECS/Registry
// dependency of its own -- callers with a live Registry supply a lookup that
// instantiates/inspects each candidate prefab as needed (and can cache
// results); tests supply a plain map-backed lambda with no ECS involved at
// all.
using SocketLookup = std::function<std::optional<SocketInfo>(std::uint32_t prefab_id)>;

// One piece placed into the generated layout, at world_offset (added to each
// of the piece's own PieceCell::offset values to get that cell's world
// position).
struct PlacedPiece
{
    std::uint32_t piece_id = 0;
    Vec2 world_offset;
};

// One connection between two placed pieces' sockets -- either a phase-1
// growth edge or a phase-2 loopback edge (DungeonLayout doesn't distinguish
// the two after the fact; GenerateDungeon only needs the distinction
// internally to pick lock candidates, see DungeonStitcher.cpp).
struct SocketConnection
{
    std::size_t piece_a = 0;
    std::size_t piece_b = 0;
    Vec2 cell_a; // world-space cell of piece_a's socket
    Vec2 cell_b; // world-space cell of piece_b's socket
};

// One lock-and-key gate: edge is a bridge connection on the entrance-to-exit
// path (see DungeonStitcher.cpp's Phase 4) narratively gated by lock_type,
// solvable by finding key_tag in the room at key_room_index -- guaranteed
// reachable from the entrance without crossing `edge` or any
// earlier-processed lock. No in-world lock/key entity is spawned by this
// struct alone; it's an abstract, verified-solvable annotation for later
// systems (items/interaction) to consume.
struct LockAnnotation
{
    SocketConnection edge;
    std::string lock_type;
    std::string key_tag;
    std::size_t key_room_index = 0;
};

// A socket left unconnected by generation -- a dead end. fallback_prefab_id
// (from the socket's own SocketInfo, 0 if none) is what a renderer should
// swap in instead of leaving a dangling doorway.
struct DeadEndSocket
{
    std::size_t piece_index = 0;
    Vec2 world_cell;
    EdgeDirection edge = EdgeDirection::North;
    std::uint32_t fallback_prefab_id = 0;
};

struct DungeonLayout
{
    std::vector<PlacedPiece> pieces;
    std::vector<SocketConnection> connections;
    std::vector<LockAnnotation> locks;
    std::vector<DeadEndSocket> dead_ends;
};

// Room-count/loopback-count targets are drawn uniformly from
// dungeon.room_count_min/max and loopback_count_min/max respectively (see
// Dungeon.h); loopback count is best-effort -- geometric adjacency may not
// always allow hitting the exact target.
//
// Generates a dungeon layout: grows a connected tree of pieces from a single
// Entrance to a single Exit (Phase 1 -- connectivity is guaranteed by
// construction, not a separate check), adds loopback connections for
// multiple paths (Phase 2), resolves unused sockets as dead ends (Phase 3),
// and places dungeon.locks as solvable lock/key gates on bridge connections
// of the entrance-to-exit path (Phase 4). Throws DungeonError if no Entrance/
// Exit piece is available in dungeon.pieces (filtered against library and
// area_tag), or if the target room count can't be reached with an Exit
// placed within a bounded attempt budget.
DungeonLayout GenerateDungeon(const Dungeon& dungeon, const PieceLibrary& library, const SocketLookup& socket_lookup,
                              std::uint64_t seed);

} // namespace psr
