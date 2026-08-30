#pragma once

#include "Engine/ECS/TypeReflection.h"
#include "Engine/Math/Vec2.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace psr {

// What kind of room a piece represents. Entrance/Exit are structurally
// special -- a dungeon's generation always places exactly one of each (see
// DungeonStitcher) -- everything else is an ordinary pool member filtered by
// a Dungeon definition's piece-ref list.
enum class PieceCategory
{
    Corridor,
    Room,
    Vault,
    BossArena,
    Entrance,
    Exit
};

template <> struct EnumNames<PieceCategory>
{
    static constexpr std::array<std::pair<std::string_view, PieceCategory>, 6> kValues{{
        {"corridor", PieceCategory::Corridor},
        {"room", PieceCategory::Room},
        {"vault", PieceCategory::Vault},
        {"boss_arena", PieceCategory::BossArena},
        {"entrance", PieceCategory::Entrance},
        {"exit", PieceCategory::Exit},
    }};
};

// Which border edge of a cell a socket sits on. Purely a placement-time
// property (see PieceCellPrefab::edge) -- not stored on the socket prefab
// itself, since the same socket prefab can be stamped facing any direction.
enum class EdgeDirection
{
    North,
    East,
    South,
    West
};

template <> struct EnumNames<EdgeDirection>
{
    static constexpr std::array<std::pair<std::string_view, EdgeDirection>, 4> kValues{{
        {"north", EdgeDirection::North},
        {"east", EdgeDirection::East},
        {"south", EdgeDirection::South},
        {"west", EdgeDirection::West},
    }};
};

inline Vec2 EdgeDirectionOffset(EdgeDirection edge)
{
    switch (edge)
    {
    case EdgeDirection::North:
        return Vec2{0, -1};
    case EdgeDirection::East:
        return Vec2{1, 0};
    case EdgeDirection::South:
        return Vec2{0, 1};
    case EdgeDirection::West:
        return Vec2{-1, 0};
    }
    return Vec2{};
}

inline EdgeDirection OppositeEdge(EdgeDirection edge)
{
    switch (edge)
    {
    case EdgeDirection::North:
        return EdgeDirection::South;
    case EdgeDirection::East:
        return EdgeDirection::West;
    case EdgeDirection::South:
        return EdgeDirection::North;
    case EdgeDirection::West:
        return EdgeDirection::East;
    }
    return EdgeDirection::North;
}

// One entity prefab stamped into a PieceCell -- the floor/decoration visual
// for that cell, mirroring UnnamedRoguelike's FeatureCell::prefabs idiom
// exactly: a cell's appearance comes entirely from its stamped prefabs' own
// RenderableComponent, never from data embedded here. Sockets are no longer
// stamped prefabs (see PieceSocket below), so this struct carries no
// per-placement override fields anymore. No retained "name string" field:
// this project resolves a hashed NameId back to its authored label via the
// global NameIdRegistry (see Engine/ECS/NameIdRegistry.h) instead of a
// per-struct _string field -- PieceLibraryFile.cpp's ReadNameId/
// WriteCellPrefab register/look it up.
struct PieceCellPrefab
{
    std::uint32_t prefab_id = 0;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&PieceCellPrefab::prefab_id>("prefab_id");
    }
};

// One occupied cell of a piece's footprint, local to the piece's origin.
// Membership in DungeonPiece::cells (not a fixed width/height array) is what
// defines the footprint's shape -- sparse and arbitrary, never required to be
// rectangular.
struct PieceCell
{
    Vec2 offset;
    std::vector<PieceCellPrefab> prefabs;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&PieceCell::offset>("offset");
        v.template Field<&PieceCell::prefabs>("prefabs");
    }
};

// A connection point on a piece's border, authored directly on the piece --
// not a stamped entity prefab (a socket has no visual of its own; the two
// pieces' ordinary cell prefabs meeting at the border provide the visual
// continuity). cell_offset+edge place it the same way PieceCellPrefab::edge
// used to; tags describes what this socket *is*, connects_to_tags what it
// *accepts* -- DungeonStitcher connects socket A to B when either's
// connects_to_tags intersects the other's tags (a one-way filter checked
// both directions, not a symmetric tags-to-tags match). fallback_prefab_id
// is stamped in by DungeonInstantiator when generation leaves this socket a
// dead end (0 means leave the cell as-is instead).
struct PieceSocket
{
    Vec2 cell_offset;
    EdgeDirection edge = EdgeDirection::North;
    std::vector<std::string> tags;
    std::vector<std::string> connects_to_tags;
    std::uint32_t fallback_prefab_id = 0;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&PieceSocket::cell_offset>("cell_offset");
        v.template Field<&PieceSocket::edge>("edge");
        v.template Field<&PieceSocket::tags>("tags");
        v.template Field<&PieceSocket::connects_to_tags>("connects_to_tags");
        v.template Field<&PieceSocket::fallback_prefab_id>("fallback_prefab_id");
    }
};

// One enemy spawn point, piece-authored like PieceSocket -- cell_offset
// places it the same way. wave groups spawns into ordered waves: the lowest
// wave number present on a piece spawns immediately at dungeon generation;
// each higher wave spawns only once every entity from the previous wave (for
// that piece's placement) has died (see SpawnWaveSystem). Wave numbers need
// not be contiguous -- pieces are grouped and sorted by whatever values are
// authored. Each entry is a single specific prefab, not a weighted pool --
// author multiple entries at the same cell/wave for variety.
struct PieceSpawn
{
    Vec2 cell_offset;
    std::uint32_t prefab_id = 0;
    int wave = 0;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&PieceSpawn::cell_offset>("cell_offset");
        v.template Field<&PieceSpawn::prefab_id>("prefab_id");
        v.template Field<&PieceSpawn::wave>("wave");
    }
};

// One authored dungeon piece: a room/corridor/vault/etc. footprint built from
// stamped entity prefabs, socket-tagged at its borders so DungeonStitcher can
// connect it to compatible neighbours. area_tag is a plain filter string for
// now (e.g. "Forest") -- not tied to a real Area/Biome entity, since M3.2
// doesn't exist yet; see docs/ROADMAP.md's M4 notes for why.
struct DungeonPiece
{
    std::uint32_t id = 0;
    std::string id_string;
    std::string name;
    std::string area_tag;
    PieceCategory category = PieceCategory::Room;
    std::vector<PieceCell> cells;
    std::vector<PieceSocket> sockets;
    std::vector<PieceSpawn> spawns;
};

} // namespace psr
