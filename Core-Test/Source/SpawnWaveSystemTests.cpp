#include "Engine/Dungeon/SpawnWaveSystem.h"

#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SpawnWaveComponent.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace {

using namespace psr;

// Throwaway test fixture prefab -- not real content, per CLAUDE.md's
// test-fixture carve-out. Mirrors DungeonInstantiatorTests.cpp's own
// EnemyMarker/TestEntityLoader pattern.
struct EnemyMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<EnemyMarker>(ctx).func<&psr::CloneComponent<EnemyMarker>>("clone"_hs);
    }
};

constexpr std::uint32_t kEnemyPrefab = 1;

class TestEntityLoader : public IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity enemy = prefab_registry.create();
        prefab_registry.emplace<EnemyMarker>(enemy);
        out_prefab_ids.emplace(kEnemyPrefab, enemy);
    }
};

// Creates a live enemy entity tagged for (group_id, wave) at world_cell,
// mirroring the stamping DungeonInstantiator/SpawnWaveSystem itself do.
entt::entity SpawnTrackedEnemy(Registry& registry, Grid& grid, Vec2 world_cell, std::uint32_t group_id, int wave)
{
    const entt::entity entity = registry.CreateEntity(kEnemyPrefab);
    registry.Emplace<Position>(entity, Position{world_cell});
    grid.AddEntity(world_cell, entity);
    registry.Emplace<SpawnWaveComponent>(entity, SpawnWaveComponent{group_id, wave});
    return entity;
}

} // namespace

TEST_CASE("SpawnWaveSystem spawns the next wave only once every current-wave entity dies", "[SpawnWaveSystem]")
{
    Registry registry;
    EnemyMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid(2, 1);
    const entt::entity first = SpawnTrackedEnemy(registry, grid, Vec2{0, 0}, 0, 0);
    const entt::entity second = SpawnTrackedEnemy(registry, grid, Vec2{0, 0}, 0, 0);

    std::unordered_map<std::uint32_t, int> initial_counts{{0, 2}};
    std::vector<PendingSpawnWave> pending;
    pending.push_back(PendingSpawnWave{0, 1, {PendingSpawnEntry{Vec2{1, 0}, kEnemyPrefab}}});

    SpawnWaveSystem system(registry, grid, initial_counts, pending);

    registry.DestroyEntity(first);
    // Only one of two wave-0 entities died -- wave 1 must not have spawned yet.
    CHECK(grid.GetEntities(Vec2{1, 0}).empty());

    registry.DestroyEntity(second);
    // The last wave-0 entity died -- wave 1 spawns now.
    REQUIRE(grid.GetEntities(Vec2{1, 0}).size() == 1);
    const entt::entity wave1_entity = grid.GetEntities(Vec2{1, 0})[0];
    CHECK(registry.HasComponent<EnemyMarker>(wave1_entity));
    REQUIRE(registry.HasComponent<SpawnWaveComponent>(wave1_entity));
    CHECK(registry.GetComponent<SpawnWaveComponent>(wave1_entity).group_id == 0);
    CHECK(registry.GetComponent<SpawnWaveComponent>(wave1_entity).wave == 1);
}

TEST_CASE("SpawnWaveSystem stays quiet once a group has no more pending waves", "[SpawnWaveSystem]")
{
    Registry registry;
    EnemyMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid(1, 1);
    const entt::entity only = SpawnTrackedEnemy(registry, grid, Vec2{0, 0}, 0, 0);

    std::unordered_map<std::uint32_t, int> initial_counts{{0, 1}};
    SpawnWaveSystem system(registry, grid, initial_counts, {});

    registry.DestroyEntity(only);

    // No pending waves for group 0 -- nothing else should have spawned. (The
    // dead entity's own stale handle stays in the Grid cell: DestroyEntity
    // alone doesn't remove it -- that's DeathSystem's job, not exercised
    // here -- so the assertion is "still exactly the one handle", not empty.)
    CHECK(grid.GetEntities(Vec2{0, 0}).size() == 1);
}

TEST_CASE("SpawnWaveSystem invokes on_spawned for a later-wave spawn", "[SpawnWaveSystem]")
{
    Registry registry;
    EnemyMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid(2, 1);
    const entt::entity only = SpawnTrackedEnemy(registry, grid, Vec2{0, 0}, 0, 0);

    std::unordered_map<std::uint32_t, int> initial_counts{{0, 1}};
    std::vector<PendingSpawnWave> pending;
    pending.push_back(PendingSpawnWave{0, 1, {PendingSpawnEntry{Vec2{1, 0}, kEnemyPrefab}}});

    std::vector<entt::entity> spawned;
    SpawnWaveSystem system(registry, grid, initial_counts, pending,
                           [&](entt::entity entity) { spawned.push_back(entity); });

    registry.DestroyEntity(only);

    REQUIRE(spawned.size() == 1);
    CHECK(spawned.front() == grid.GetEntities(Vec2{1, 0})[0]);
}

TEST_CASE("SpawnWaveSystem skips an invalid prefab id in a pending wave", "[SpawnWaveSystem]")
{
    Registry registry;
    EnemyMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid(2, 1);
    const entt::entity only = SpawnTrackedEnemy(registry, grid, Vec2{0, 0}, 0, 0);

    std::unordered_map<std::uint32_t, int> initial_counts{{0, 1}};
    std::vector<PendingSpawnWave> pending;
    pending.push_back(PendingSpawnWave{0, 1, {PendingSpawnEntry{Vec2{1, 0}, /*prefab_id=*/999}}});

    SpawnWaveSystem system(registry, grid, initial_counts, pending);

    registry.DestroyEntity(only);

    // The pending wave's only entry has an unregistered prefab id -- nothing stamps.
    CHECK(grid.GetEntities(Vec2{1, 0}).empty());
}
