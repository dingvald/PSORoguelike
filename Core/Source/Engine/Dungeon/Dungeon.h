#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// One piece the stitcher may draw from for a particular Dungeon, with its
// weight/max-occurrence scoped to this dungeon only -- the same piece can be
// referenced by another Dungeon with different values, so these don't live on
// DungeonPiece itself. No retained "name string" for piece_id: resolved via
// the global NameIdRegistry, same convention as PieceCellPrefab::prefab_id
// (see DungeonPiece.h).
struct DungeonPieceRef
{
    std::uint32_t piece_id = 0;
    float weight = 1.0f;
    int max_occurrences = 0; // 0 = unlimited, scoped to this dungeon only

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&DungeonPieceRef::piece_id>("piece_id");
        v.template Field<&DungeonPieceRef::weight>("weight");
        v.template Field<&DungeonPieceRef::max_occurrences>("max_occurrences");
    }
};

// One authored dungeon definition: which pieces are eligible (and how often/
// likely each is used), how many rooms to generate, how many loopback
// connections to add on top of the growth tree, how many lock gates to place
// (see DungeonStitcher.h's Phase 4 -- each gate's unlock mechanic comes from
// whichever DungeonPiece ends up gated behind it, via its own
// preferred_unlock_condition, not from this struct), and this dungeon's
// door/switch prefab look. Consumed by DungeonStitcher::GenerateDungeon
// against a PieceLibrary.
struct Dungeon
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    std::string area_tag;
    std::vector<DungeonPieceRef> pieces;
    int room_count_min = 10;
    int room_count_max = 15;
    int loopback_count_min = 0;
    int loopback_count_max = 2;
    int lock_count = 0;

    // This dungeon's door/switch look -- one per dungeon rather than
    // per-socket or per-piece, since a connected socket's two sides can
    // belong to different pieces with no natural tie-break for whose door
    // prefab should win. unlocked_door_prefab_id is stamped at every
    // connected socket bordering a Room/Vault/BossArena piece;
    // locked_door_prefab_id replaces it wherever DungeonStitcher placed a
    // lock; switch_prefab_id is stamped at a Switch-condition lock's
    // generator-picked cell (see LockAnnotation::switch_cell). 0 leaves the
    // corresponding stamp a no-op, same convention as every other prefab-id
    // field (see PieceSocket::fallback_prefab_id).
    std::uint32_t unlocked_door_prefab_id = 0;
    std::uint32_t locked_door_prefab_id = 0;
    std::uint32_t switch_prefab_id = 0;
};

} // namespace psr
