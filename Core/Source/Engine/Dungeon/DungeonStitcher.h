#pragma once

#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonPiece.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// One piece placed into the generated layout, at world_offset (added to each
// of the piece's own PieceCell::offset values to get that cell's world
// position).
struct PlacedPiece
{
    std::uint32_t piece_id = 0;
    Vec2 world_offset;
    PieceTransform transform;
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
// (from the socket's own PieceSocket::fallback_prefab_id, 0 if none) is what
// a renderer should stamp in instead of leaving a dangling doorway.
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
// construction, not a separate check), caps any dead-end Corridor socket
// with a terminal Room or Vault so a hallway isn't a dead end in name only
// (Phase 1.5, best-effort) -- preferring a piece tagged "dead_end"
// (DungeonPiece::tags) and falling back to any Room/Vault if none of the
// tagged ones fit, with the capped piece's own remaining sockets collapsed
// straight to fallback-stamped dead ends rather than fed back into Phase
// 2/3 -- adds loopback connections for multiple paths (Phase 2), resolves
// remaining unused sockets as dead ends (Phase 3), and places dungeon.locks
// as solvable lock/key gates on bridge connections of the entrance-to-exit
// path (Phase 4). Sockets are read directly off each piece's own
// DungeonPiece::sockets -- no ECS/Registry lookup involved, since a socket
// is piece-authored data, not a stamped entity. Throws DungeonError if no
// Entrance/Exit piece is available in dungeon.pieces (filtered against
// library and area_tag), or if the target room count can't be reached with
// an Exit placed within a bounded attempt budget.
DungeonLayout GenerateDungeon(const Dungeon& dungeon, const PieceLibrary& library, std::uint64_t seed);

} // namespace psr
