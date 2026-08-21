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

// One lock-and-key requirement authored for a dungeon: place `count`
// independently-solvable locks tagged lock_type, each gating one bridge
// connection on the entrance-to-exit path with its key placed somewhere
// already reachable before it (see DungeonStitcher.h's Phase 4). lock_type is
// a freeform author-defined tag ("red_key", "switch", ...), not a fixed enum
// -- the actual lock/key gameplay mechanic doesn't exist yet (items/
// interaction land in later milestones), so this only needs to round-trip as
// data for now.
struct DungeonLockConfig
{
    std::string lock_type;
    int count = 1;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&DungeonLockConfig::lock_type>("lock_type");
        v.template Field<&DungeonLockConfig::count>("count");
    }
};

// One authored dungeon definition: which pieces are eligible (and how often/
// likely each is used), how many rooms to generate, how many loopback
// connections to add on top of the growth tree, and what lock/key gating to
// place. Consumed by DungeonStitcher::GenerateDungeon against a PieceLibrary.
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
    std::vector<DungeonLockConfig> locks;
};

} // namespace psr
