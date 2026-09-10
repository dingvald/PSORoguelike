#include "Engine/Dungeon/DungeonInstantiator.h"

#include "Engine/Actions/MoveEvent.h"
#include "Engine/Dungeon/DoorComponent.h"
#include "Engine/Dungeon/RoomClearDoorSystem.h"
#include "Engine/Dungeon/SwitchComponent.h"
#include "Engine/Dungeon/SwitchTriggerSystem.h"
#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SpawnWaveComponent.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

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

struct EnemyMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<EnemyMarker>(ctx).func<&psr::CloneComponent<EnemyMarker>>("clone"_hs);
    }
};

// Distinct from DoorMarker above (an unrelated "second stamped cell prefab"
// fixture) -- these stand in for a Dungeon's own locked/unlocked door and
// switch prefabs (see Dungeon::locked_door_prefab_id/unlocked_door_prefab_id/
// switch_prefab_id), stamped by InstantiateDungeon's new door/switch logic
// rather than authored per-cell.
struct LockedDoorMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<LockedDoorMarker>(ctx).func<&psr::CloneComponent<LockedDoorMarker>>("clone"_hs);
    }
};

struct UnlockedDoorMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<UnlockedDoorMarker>(ctx).func<&psr::CloneComponent<UnlockedDoorMarker>>("clone"_hs);
    }
};

struct SwitchMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<SwitchMarker>(ctx).func<&psr::CloneComponent<SwitchMarker>>("clone"_hs);
    }
};

constexpr std::uint32_t kFloorPrefab = 1;
constexpr std::uint32_t kDoorPrefab = 2;
constexpr std::uint32_t kFallbackPrefab = 3;
constexpr std::uint32_t kEnemyPrefab = 4;
constexpr std::uint32_t kLockedDoorPrefab = 5;
constexpr std::uint32_t kUnlockedDoorPrefab = 6;
constexpr std::uint32_t kSwitchPrefab = 7;

class TestEntityLoader : public IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
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

        entt::entity enemy = prefab_registry.create();
        prefab_registry.emplace<EnemyMarker>(enemy);
        out_prefab_ids.emplace(kEnemyPrefab, enemy);

        entt::entity locked_door = prefab_registry.create();
        prefab_registry.emplace<LockedDoorMarker>(locked_door);
        out_prefab_ids.emplace(kLockedDoorPrefab, locked_door);

        entt::entity unlocked_door = prefab_registry.create();
        prefab_registry.emplace<UnlockedDoorMarker>(unlocked_door);
        out_prefab_ids.emplace(kUnlockedDoorPrefab, unlocked_door);

        entt::entity switch_entity = prefab_registry.create();
        prefab_registry.emplace<SwitchMarker>(switch_entity);
        out_prefab_ids.emplace(kSwitchPrefab, switch_entity);
    }
};

// One piece: a floor-only cell at local (0,0), and a floor+door cell at
// local (1,0) -- exercises a cell with more than one ordinary stamped
// prefab. No PieceSocket here; see MakePieceWithSocket below for the
// dead-end-fallback tests, since a socket is piece-authored data now, not a
// stamped prefab occupying a cell slot.
DungeonPiece MakeTwoCellPiece(std::uint32_t id)
{
    DungeonPiece piece;
    piece.id = id;
    piece.category = PieceCategory::Room;

    PieceCell floor_only;
    floor_only.offset = Vec2{0, 0};
    floor_only.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
    piece.cells.push_back(floor_only);

    PieceCell floor_and_door;
    floor_and_door.offset = Vec2{1, 0};
    floor_and_door.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
    floor_and_door.prefabs.push_back(PieceCellPrefab{kDoorPrefab});
    piece.cells.push_back(floor_and_door);

    return piece;
}

// One piece: a floor-only cell at local (0,0), and a floor-only cell at
// local (1,0) carrying a socket (edge East, no visual of its own) -- for
// exercising InstantiateDungeon's dead-end fallback stamp, which reads
// DungeonPiece::sockets directly rather than anything in cell.prefabs.
DungeonPiece MakePieceWithSocket(std::uint32_t id)
{
    DungeonPiece piece;
    piece.id = id;
    piece.category = PieceCategory::Room;

    PieceCell floor_only_a;
    floor_only_a.offset = Vec2{0, 0};
    floor_only_a.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
    piece.cells.push_back(floor_only_a);

    PieceCell floor_only_b;
    floor_only_b.offset = Vec2{1, 0};
    floor_only_b.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
    piece.cells.push_back(floor_only_b);

    PieceSocket socket;
    socket.cell_offset = Vec2{1, 0};
    socket.edge = EdgeDirection::East;
    piece.sockets.push_back(socket);

    return piece;
}

// One floor-only cell at local (0,0), with the given spawns attached --
// for exercising InstantiateDungeon's wave stamping/deferral, which reads
// DungeonPiece::spawns directly, independent of cell.prefabs.
DungeonPiece MakePieceWithSpawns(std::uint32_t id, std::vector<PieceSpawn> spawns)
{
    DungeonPiece piece;
    piece.id = id;
    piece.category = PieceCategory::Room;

    PieceCell floor_only;
    floor_only.offset = Vec2{0, 0};
    floor_only.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
    piece.cells.push_back(floor_only);

    piece.spawns = std::move(spawns);
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

TEST_CASE("ComputeDungeonBounds accounts for a placed piece's transform", "[DungeonInstantiator]")
{
    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}, PieceTransform{1, false}});

    const Rect bounds = ComputeDungeonBounds(layout, library);

    // Authored cells (0,0)/(1,0) rotate 90 degrees clockwise to (0,0)/(0,1).
    CHECK(bounds.origin == Vec2{0, 0});
    CHECK(bounds.size == Vec2{1, 2});
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

TEST_CASE("InstantiateDungeon applies a placed piece's transform before translating cells", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}, PieceTransform{1, false}});

    Grid grid(2, 2);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    // Authored cells (0,0) and (1,0) rotate 90 degrees clockwise to (0,0)
    // and (0,1) instead of the untransformed piece's (0,0)/(1,0).
    REQUIRE(grid.GetEntities(Vec2{0, 0}).size() == 1);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{0, 0})[0]));

    REQUIRE(grid.GetEntities(Vec2{0, 1}).size() == 2);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{0, 1})[0]));
    CHECK(registry.HasComponent<DoorMarker>(grid.GetEntities(Vec2{0, 1})[1]));

    CHECK(result.room_map.GetRoom(Vec2{0, 1}) == 0u);
}

TEST_CASE("InstantiateDungeon additionally stamps a dead end socket's fallback prefab", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    FallbackMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakePieceWithSocket(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});
    layout.dead_ends.push_back(DeadEndSocket{0, Vec2{1, 0}, EdgeDirection::East, kFallbackPrefab});

    Grid grid(2, 1);
    InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{1, 0})[0]));
    // The dead end's fallback stamps in addition to the cell's own prefabs.
    CHECK(registry.HasComponent<FallbackMarker>(grid.GetEntities(Vec2{1, 0})[1]));
}

TEST_CASE("InstantiateDungeon leaves a dead end unstamped when fallback_prefab_id is 0", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakePieceWithSocket(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});
    layout.dead_ends.push_back(DeadEndSocket{0, Vec2{1, 0}, EdgeDirection::East, /*fallback_prefab_id=*/0});

    Grid grid(2, 1);
    InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    // Only the floor prefab stamps -- the dead end's socket has no fallback.
    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 1);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{1, 0})[0]));
}

TEST_CASE("InstantiateDungeon stamps a piece's only (wave 0) spawn immediately", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    FallbackMarker::Register(registry.GetMetaContext());
    EnemyMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceSpawn spawn;
    spawn.cell_offset = Vec2{0, 0};
    spawn.prefab_id = kEnemyPrefab;
    spawn.wave = 0;

    PieceLibrary library{{MakePieceWithSpawns(10, {spawn})}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});

    Grid grid(1, 1);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    // Floor prefab + the wave-0 enemy both land on the same cell.
    REQUIRE(grid.GetEntities(Vec2{0, 0}).size() == 2);
    const entt::entity enemy = grid.GetEntities(Vec2{0, 0})[1];
    CHECK(registry.HasComponent<EnemyMarker>(enemy));
    REQUIRE(registry.HasComponent<SpawnWaveComponent>(enemy));
    CHECK(registry.GetComponent<SpawnWaveComponent>(enemy).group_id == 0);
    CHECK(registry.GetComponent<SpawnWaveComponent>(enemy).wave == 0);

    REQUIRE(result.initial_wave_counts.count(0) == 1);
    CHECK(result.initial_wave_counts.at(0) == 1);
    CHECK(result.pending_spawn_waves.empty());
}

TEST_CASE("InstantiateDungeon stamps only wave 0 and defers wave 1", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    FallbackMarker::Register(registry.GetMetaContext());
    EnemyMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceSpawn wave0;
    wave0.cell_offset = Vec2{0, 0};
    wave0.prefab_id = kEnemyPrefab;
    wave0.wave = 0;

    PieceSpawn wave1;
    wave1.cell_offset = Vec2{0, 0};
    wave1.prefab_id = kEnemyPrefab;
    wave1.wave = 1;

    PieceLibrary library{{MakePieceWithSpawns(10, {wave0, wave1})}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});

    Grid grid(1, 1);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    // Only the floor prefab + wave-0 enemy stamp now -- wave 1 stays pending.
    REQUIRE(grid.GetEntities(Vec2{0, 0}).size() == 2);
    REQUIRE(result.initial_wave_counts.count(0) == 1);
    CHECK(result.initial_wave_counts.at(0) == 1);

    REQUIRE(result.pending_spawn_waves.size() == 1);
    const PendingSpawnWave& pending = result.pending_spawn_waves.front();
    CHECK(pending.group_id == 0);
    CHECK(pending.wave == 1);
    REQUIRE(pending.entries.size() == 1);
    CHECK(pending.entries.front().prefab_id == kEnemyPrefab);
    CHECK(pending.entries.front().world_cell == Vec2{0, 0});
}

TEST_CASE("InstantiateDungeon invokes on_spawned for a first-wave spawn, not for cell prefabs", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    FallbackMarker::Register(registry.GetMetaContext());
    EnemyMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceSpawn spawn;
    spawn.cell_offset = Vec2{0, 0};
    spawn.prefab_id = kEnemyPrefab;
    spawn.wave = 0;

    PieceLibrary library{{MakePieceWithSpawns(10, {spawn})}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});

    Grid grid(1, 1);
    std::vector<entt::entity> spawned;
    InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid,
                       [&](entt::entity entity) { spawned.push_back(entity); });

    // Only the wave-0 enemy triggers on_spawned -- the cell's own floor
    // prefab is static dungeon furniture, not a creature.
    REQUIRE(spawned.size() == 1);
    CHECK(registry.HasComponent<EnemyMarker>(spawned.front()));
}

TEST_CASE("InstantiateDungeon tags every cell with its placed piece's index in room_map", "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});  // piece_index 0, grid cells (0,0)/(1,0)
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 1}});  // piece_index 1, grid cells (0,1)/(1,1)

    Grid grid(2, 2);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    REQUIRE(result.room_map.GetRoom(Vec2{0, 0}) == 0u);
    REQUIRE(result.room_map.GetRoom(Vec2{1, 0}) == 0u);
    REQUIRE(result.room_map.GetRoom(Vec2{0, 1}) == 1u);
    REQUIRE(result.room_map.GetRoom(Vec2{1, 1}) == 1u);

    // Untagged tiles (outside every placed piece's footprint) stay unmapped.
    CHECK_FALSE(result.room_map.GetRoom(Vec2{5, 5}).has_value());
}

TEST_CASE("InstantiateDungeon builds room_adjacency from layout.connections in both directions",
          "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    DoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});  // piece_index 0
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 1}});  // piece_index 1
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 2}});  // piece_index 2, unconnected
    layout.connections.push_back(SocketConnection{0, 1, Vec2{0, 0}, Vec2{0, 1}});

    Grid grid(2, 3);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    REQUIRE(result.room_adjacency.size() == 3);
    CHECK(result.room_adjacency[0] == std::vector<std::uint32_t>{1});
    CHECK(result.room_adjacency[1] == std::vector<std::uint32_t>{0});
    CHECK(result.room_adjacency[2].empty());
}

TEST_CASE("InstantiateDungeon leaves both spawn maps untouched for a piece with no spawns", "[DungeonInstantiator]")
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

    Grid grid(2, 1);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    CHECK(result.initial_wave_counts.empty());
    CHECK(result.pending_spawn_waves.empty());
}

// -- Doors, switches, and locks -------------------------------------------

namespace {

    // One floor-only cell at local (0,0), category-parameterized -- for
    // exercising InstantiateDungeon's unlocked-door-on-connection logic,
    // which only cares about PieceCategory, not footprint shape.
    DungeonPiece MakeSimplePiece(std::uint32_t id, PieceCategory category)
    {
        DungeonPiece piece;
        piece.id = id;
        piece.category = category;

        PieceCell floor_only;
        floor_only.offset = Vec2{0, 0};
        floor_only.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
        piece.cells.push_back(floor_only);
        return piece;
    }

    // Finds the one entity on tile carrying TComponent -- insertion order
    // among a tile's occupants (door vs. floor vs. ...) is an implementation
    // detail these tests don't want to pin down.
    template <typename TComponent> entt::entity FindWith(const Registry& registry, const Grid& grid, Vec2 tile)
    {
        for (entt::entity entity : grid.GetEntities(tile))
            if (registry.HasComponent<TComponent>(entity))
                return entity;
        return entt::null;
    }

} // namespace

TEST_CASE("InstantiateDungeon stamps a locked door for a RoomCleared lock, unlocked once its room clears",
          "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    EnemyMarker::Register(registry.GetMetaContext());
    LockedDoorMarker::Register(registry.GetMetaContext());
    UnlockedDoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceSpawn spawn;
    spawn.cell_offset = Vec2{0, 0};
    spawn.prefab_id = kEnemyPrefab;
    spawn.wave = 0;

    PieceLibrary library{{MakeTwoCellPiece(10), MakePieceWithSpawns(20, {spawn})}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}}); // piece_index 0: outside, cells (0,0)/(1,0)
    layout.pieces.push_back(PlacedPiece{20, Vec2{2, 0}}); // piece_index 1: gated room, cell (2,0)
    layout.connections.push_back(SocketConnection{0, 1, Vec2{1, 0}, Vec2{2, 0}});
    layout.locks.push_back(LockAnnotation{SocketConnection{0, 1, Vec2{1, 0}, Vec2{2, 0}}, /*inside_room_index=*/1,
                                          DoorUnlockCondition::RoomCleared, 0, Vec2{}});
    layout.locked_door_prefab_id = kLockedDoorPrefab;
    layout.unlocked_door_prefab_id = kUnlockedDoorPrefab;

    Grid grid(3, 1);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    // The lock's own edge cell_a (1,0) stamps the locked door, not the
    // generic unlocked-door pass (it's claimed by the lock already).
    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2); // floor + locked door
    const entt::entity door = FindWith<LockedDoorMarker>(registry, grid, Vec2{1, 0});
    REQUIRE(door != entt::null);
    REQUIRE(registry.HasComponent<DoorComponent>(door));
    CHECK(registry.GetComponent<DoorComponent>(door).condition == DoorUnlockCondition::RoomCleared);
    CHECK(registry.GetComponent<DoorComponent>(door).room_index == 1);

    REQUIRE(result.room_cleared_doors.count(1) == 1);
    CHECK(result.room_cleared_doors.at(1) == std::vector<entt::entity>{door});

    RoomClearDoorSystem system(registry, grid, result.initial_wave_counts, result.pending_spawn_waves,
                               result.room_cleared_doors);

    // The gated room's one enemy is still alive -- door stays locked.
    CHECK(FindWith<LockedDoorMarker>(registry, grid, Vec2{1, 0}) == door);

    const entt::entity enemy = FindWith<EnemyMarker>(registry, grid, Vec2{2, 0});
    REQUIRE(enemy != entt::null);
    registry.DestroyEntity(enemy);

    // Killing the room's last enemy unlocks the door in place.
    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2);
    CHECK(FindWith<LockedDoorMarker>(registry, grid, Vec2{1, 0}) == entt::null);
    CHECK(FindWith<UnlockedDoorMarker>(registry, grid, Vec2{1, 0}) != entt::null);
}

TEST_CASE("RoomClearDoorSystem unlocks a RoomCleared door immediately when its room has no spawns",
          "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    LockedDoorMarker::Register(registry.GetMetaContext());
    UnlockedDoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10), MakeSimplePiece(20, PieceCategory::Room)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});
    layout.pieces.push_back(PlacedPiece{20, Vec2{2, 0}}); // gated room, no spawns
    layout.connections.push_back(SocketConnection{0, 1, Vec2{1, 0}, Vec2{2, 0}});
    layout.locks.push_back(LockAnnotation{SocketConnection{0, 1, Vec2{1, 0}, Vec2{2, 0}}, /*inside_room_index=*/1,
                                          DoorUnlockCondition::RoomCleared, 0, Vec2{}});
    layout.locked_door_prefab_id = kLockedDoorPrefab;
    layout.unlocked_door_prefab_id = kUnlockedDoorPrefab;

    Grid grid(3, 1);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    RoomClearDoorSystem system(registry, grid, result.initial_wave_counts, result.pending_spawn_waves,
                               result.room_cleared_doors);

    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2);
    CHECK(FindWith<LockedDoorMarker>(registry, grid, Vec2{1, 0}) == entt::null);
    CHECK(FindWith<UnlockedDoorMarker>(registry, grid, Vec2{1, 0}) != entt::null);
}

TEST_CASE("SwitchTriggerSystem unlocks a Switch lock's door when its switch -- in a different room -- is triggered",
          "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    LockedDoorMarker::Register(registry.GetMetaContext());
    UnlockedDoorMarker::Register(registry.GetMetaContext());
    SwitchMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeTwoCellPiece(10), MakeSimplePiece(20, PieceCategory::Room)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{10, Vec2{0, 0}});  // piece_index 0: door's own room, cells (0,0)/(1,0)
    layout.pieces.push_back(PlacedPiece{20, Vec2{2, 0}});  // piece_index 1: gated room, cell (2,0)
    layout.connections.push_back(SocketConnection{0, 1, Vec2{1, 0}, Vec2{2, 0}});
    LockAnnotation lock{SocketConnection{0, 1, Vec2{1, 0}, Vec2{2, 0}}, /*inside_room_index=*/1,
                       DoorUnlockCondition::Switch, /*switch_room_index=*/0, /*switch_cell=*/Vec2{5, 0}};
    layout.locks.push_back(lock);
    layout.locked_door_prefab_id = kLockedDoorPrefab;
    layout.unlocked_door_prefab_id = kUnlockedDoorPrefab;
    layout.switch_prefab_id = kSwitchPrefab;

    // Grid must cover both the door's room and the far-off switch cell (5,0)
    // -- deliberately not part of either placed piece's own footprint, since
    // InstantiateDungeon stamps a lock's switch at its precomputed
    // switch_cell directly, independent of room membership.
    Grid grid(6, 1);
    const DungeonInstantiation result = InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2);
    const entt::entity door = FindWith<LockedDoorMarker>(registry, grid, Vec2{1, 0});
    REQUIRE(door != entt::null);
    REQUIRE(registry.HasComponent<DoorComponent>(door));
    CHECK(registry.GetComponent<DoorComponent>(door).remaining_switches == 1);

    REQUIRE(grid.GetEntities(Vec2{5, 0}).size() == 1);
    const entt::entity switch_entity = grid.GetEntities(Vec2{5, 0})[0];
    REQUIRE(registry.HasComponent<SwitchComponent>(switch_entity));
    CHECK(registry.GetComponent<SwitchComponent>(switch_entity).target_door == door);

    SwitchTriggerSystem system(registry, grid);
    const entt::entity actor = registry.CreateEntity();
    system.Subscribe(Entity(registry, actor));

    AfterMoveEvent move{Vec2{4, 0}, Vec2{5, 0}};
    Entity(registry, actor).Dispatch(move);

    CHECK(registry.GetComponent<SwitchComponent>(switch_entity).activated);
    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 2);
    CHECK(FindWith<LockedDoorMarker>(registry, grid, Vec2{1, 0}) == entt::null);
    CHECK(FindWith<UnlockedDoorMarker>(registry, grid, Vec2{1, 0}) != entt::null);
}

TEST_CASE("InstantiateDungeon stamps an unlocked door only where a connection borders a Room/Vault/BossArena piece",
          "[DungeonInstantiator]")
{
    Registry registry;
    FloorMarker::Register(registry.GetMetaContext());
    UnlockedDoorMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    PieceLibrary library{{MakeSimplePiece(30, PieceCategory::Room), MakeSimplePiece(31, PieceCategory::Room),
                          MakeSimplePiece(32, PieceCategory::Corridor), MakeSimplePiece(33, PieceCategory::Corridor)}};
    DungeonLayout layout;
    layout.pieces.push_back(PlacedPiece{30, Vec2{0, 0}}); // piece_index 0
    layout.pieces.push_back(PlacedPiece{31, Vec2{1, 0}}); // piece_index 1
    layout.pieces.push_back(PlacedPiece{32, Vec2{0, 1}}); // piece_index 2
    layout.pieces.push_back(PlacedPiece{33, Vec2{1, 1}}); // piece_index 3
    layout.connections.push_back(SocketConnection{0, 1, Vec2{0, 0}, Vec2{1, 0}});
    layout.connections.push_back(SocketConnection{2, 3, Vec2{0, 1}, Vec2{1, 1}});
    layout.unlocked_door_prefab_id = kUnlockedDoorPrefab;

    Grid grid(2, 2);
    InstantiateDungeon(layout, library, Vec2{0, 0}, registry, grid);

    // Room-to-Room connection: floor + unlocked door.
    REQUIRE(grid.GetEntities(Vec2{0, 0}).size() == 2);
    CHECK(FindWith<UnlockedDoorMarker>(registry, grid, Vec2{0, 0}) != entt::null);

    // Corridor-to-Corridor connection: floor only, no door.
    REQUIRE(grid.GetEntities(Vec2{0, 1}).size() == 1);
    CHECK(registry.HasComponent<FloorMarker>(grid.GetEntities(Vec2{0, 1})[0]));
}
