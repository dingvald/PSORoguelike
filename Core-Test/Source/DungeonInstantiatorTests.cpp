#include "Engine/Dungeon/DungeonInstantiator.h"

#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/Position.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace psr;

// Throwaway test fixture prefabs -- not real content, per CLAUDE.md's
// test-fixture carve-out.
struct FloorMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<FloorMarker>(ctx).func<&psr::CloneComponent<FloorMarker>>("clone"_hs);
    }
};

struct DoorMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<DoorMarker>(ctx).func<&psr::CloneComponent<DoorMarker>>("clone"_hs);
    }
};

struct FallbackMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<FallbackMarker>(ctx).func<&psr::CloneComponent<FallbackMarker>>("clone"_hs);
    }
};

constexpr std::uint32_t kFloorPrefab = 1;
constexpr std::uint32_t kDoorPrefab = 2;
constexpr std::uint32_t kFallbackPrefab = 3;

class TestEntityLoader : public IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry, std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity floor = prefab_registry.create();
        prefab_registry.emplace<FloorMarker>(floor);
        out_prefab_ids.emplace(kFloorPrefab, floor);

        entt::entity door = prefab_registry.create();
        prefab_registry.emplace<DoorMarker>(door);
        out_prefab_ids.emplace(kDoorPrefab, door);

        entt::entity fallback = prefab_registry.create();
        prefab_registry.emplace<FallbackMarker>(fallback);
        out_prefab_ids.emplace(kFallbackPrefab, fallback);
    }
};

// One piece: a floor-only cell at local (0,0), and a floor+door cell (the
// door facing East) at local (1,0) -- exercises a cell with more than one
// stamped prefab and a socket cell distinct from a plain floor cell.
DungeonPiece MakeTwoCellPiece(std::uint32_t id)
{
    DungeonPiece piece;
    piece.id = id;
    piece.category = PieceCategory::Room;

    PieceCell floor_only;
    floor_only.offset = Vec2{0, 0};
    floor_only.prefabs.push_back(PieceCellPrefab{kFloorPrefab, EdgeDirection::North});
    piece.cells.push_back(floor_only);

    PieceCell floor_and_door;
    floor_and_door.offset = Vec2{1, 0};
    floor_and_door.prefabs.push_back(PieceCellPrefab{kFloorPrefab, EdgeDirection::North});
    floor_and_door.prefabs.push_back(PieceCellPrefab{kDoorPrefab, EdgeDirection::East});
    piece.cells.push_back(floor_and_door);

    return piece;
}

} // namespace

TEST_CASE("ComputeDungeonBounds covers a single placed piece's cell extent", "[DungeonInstantiator]")
{
    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});

    const Rect bounds = ComputeDungeonBounds(layout, library);

    CHECK(bounds.origin == Vec2{0, 0});
    CHECK(bounds.size == Vec2{2, 1});
}

TEST_CASE("ComputeDungeonBounds spans negative world_offset placements", "[DungeonInstantiator]")
{
    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});
    layout.pieces.push_back(PlacedPiece{10, Vec2{-3, -2}});

    const Rect bounds = ComputeDungeonBounds(layout, library);

    // Second placement spans x in [-3,-2], y in [-2,-2]; first spans x in
    // [0,1], y in [0,0] -- union is x in [-3,1], y in [-2,0].
    CHECK(bounds.origin == Vec2{-3, -2});
    CHECK(bounds.size == Vec2{5, 3});
}

TEST_CASE("ComputeDungeonBounds ignores a placed piece the library can't resolve", "[DungeonInstantiator]")
{
    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{999, Vec2{5, 5}}); // unknown id

    const Rect bounds = ComputeDungeonBounds(layout, library);

    CHECK(bounds.size == Vec2{0, 0});
}

TEST_CASE("InstantiateDungeon stamps every cell's prefabs into the grid at translated positions",
          "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    FallbackMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{-3, -2}});

    const Rect bounds = ComputeDungeonBounds(layout, library);
    Grid grid(bounds.size.x, bounds.size.y);
    const Vec2 offset = -bounds.origin;

    const DungeonInstantiation result = InstantiateDungeon(layout, library, offset, registry, grid);

    // Piece cells (-3,-2) and (-2,-2) translate to grid cells (0,0) and (1,0).
    REQUIRE(grid.GetEntities(Vec2{0, 0}).size() == 1);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{0, 0})[0]));
    CHECK(registry.GetComponent<Position>(grid.GetEntities(Vec2{0, 0})[0]).tile == Vec2{0, 0});

    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{1, 0})[0]));
    CHECK(registry.HasComponent<DoorMarker>(grid.GetEntities(Vec2{1, 0})[1]));
    CHECK(registry.GetComponent<Position>(grid.GetEntities(Vec2{1, 0})[1]).tile == Vec2{1, 0});

    // Entrance tile is the first placed piece's first cell, same translation.
    CHECK(result.entrance_tile == Vec2{0, 0});
}

TEST_CASE("InstantiateDungeon substitutes a dead end's fallback prefab for its socket", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    FallbackMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});
    layout.dead_ends.push_back(DeadEndSocket{0, Vec2{1, 0}, EdgeDirection::East, kFallbackPrefab});

    Grid grid(2, 1);
    InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{1, 0})[0]));
    // The door prefab was swapped for the dead end's fallback, not stamped as-is.
    CHECK_FALSE(registry.HasComponent<DoorMarker>(grid.GetEntities(Vec2{1, 0})[1]));
    CHECK(registry.HasComponent<FallbackMarker>(grid.GetEntities(Vec2{1, 0})[1]));
}

TEST_CASE("InstantiateDungeon leaves a dead end unstamped when fallback_prefab_id is 0", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});
    layout.dead_ends.push_back(DeadEndSocket{0, Vec2{1, 0}, EdgeDirection::East, /*fallback_prefab_id=*/0});

    Grid grid(2, 1);
    InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    // Only the floor prefab stamps -- the door's dead end has no fallback.
    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 1);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{1, 0})[0]));
}
