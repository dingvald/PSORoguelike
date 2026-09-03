#include "Systems/LootDropSystem.h"

#include "Components/CurrencyComponent.h"
#include "Components/CurrencyPickupComponent.h"
#include "Components/DropTableComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"
#include "Engine/World/Grid.h"
#include "Messages/LootDropMessage.h"
#include "Messages/MesetaChangedMessage.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <unordered_map>
#include <vector>

namespace {

using namespace psr;

// Throwaway test fixture prefab -- not real content, per CLAUDE.md's
// test-fixture carve-out. Mirrors SpawnWaveSystemTests.cpp's own
// EnemyMarker/TestEntityLoader pattern, needed so Registry::CreateEntity(id)
// has something real to instantiate for a dropped item.
struct ItemMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<ItemMarker>(ctx).func<&psr::CloneComponent<ItemMarker>>("clone"_hs);
    }
};

constexpr std::uint32_t kItemPrefab = 1;
const std::uint32_t kMesetaPrefab = entt::hashed_string::value("meseta");

class TestEntityLoader : public IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity item = prefab_registry.create();
        prefab_registry.emplace<ItemMarker>(item);
        out_prefab_ids.emplace(kItemPrefab, item);

        entt::entity meseta = prefab_registry.create();
        prefab_registry.emplace<CurrencyPickupComponent>(meseta, CurrencyPickupComponent{0});
        out_prefab_ids.emplace(kMesetaPrefab, meseta);
    }
};

Entity MakeActorAt(Registry& registry, Grid& grid, Vec2 tile)
{
    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<Position>(tile);
    grid.AddEntity(tile, handle);
    return actor;
}

// Registers ItemMarker (a manual clone func, same as the existing precedent)
// and CurrencyPickupComponent (via the real ComponentSchemaRegistrar, since
// LootDropSystem needs to Registry::GetComponent<CurrencyPickupComponent> on
// a freshly-cloned "meseta" entity to overwrite its rolled amount).
void RegisterTestPrefabTypes(Registry& registry)
{
    ItemMarker::Register(registry.GetMetaContext());
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};
    CurrencyPickupComponent::Register(reg);
}

} // namespace

TEST_CASE("LootDropSystem no-ops when the hit did not defeat the target", "[LootDropSystem]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid{4, 4};
    MessageBus bus;
    std::mt19937 rng{1};

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    DropTableComponent table;
    table.entries = {LootEntry{kItemPrefab, 1.0f}};
    target.Emplace<DropTableComponent>(table);

    LootDropSystem system(registry, grid, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/5, /*is_critical=*/false, /*target_defeated=*/false};
    player.Dispatch(event);

    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 1); // only target itself, nothing dropped
}

TEST_CASE("LootDropSystem no-ops when the defeated target has no DropTableComponent", "[LootDropSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    std::mt19937 rng{1};

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});

    LootDropSystem system(registry, grid, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/5, /*is_critical=*/false, /*target_defeated=*/true};
    player.Dispatch(event);

    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 1); // no drop-table ref -- nothing dropped
    CHECK_FALSE(player.Has<CurrencyComponent>());
}

TEST_CASE("LootDropSystem spawns a ground item and publishes LootDropMessage on a lethal player hit",
          "[LootDropSystem]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid{4, 4};
    MessageBus bus;
    MessageQueue hud_queue;
    std::mt19937 rng{1};

    std::vector<std::string> loot_names;
    hud_queue.RegisterHandler<LootDropMessage>([&](const LootDropMessage& m) { loot_names.push_back(m.item_name); });
    bus.Subscribe<LootDropMessage>(hud_queue);

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    DropTableComponent table;
    table.entries = {LootEntry{kItemPrefab, 1.0f}};
    target.Emplace<DropTableComponent>(table);

    LootDropSystem system(registry, grid, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*is_critical=*/false, /*target_defeated=*/true};
    player.Dispatch(event);

    // The pre-existing target entity plus one freshly-spawned item.
    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 2);

    hud_queue.HandleQueuedMessages();
    CHECK(loot_names.size() == 1);
}

TEST_CASE("LootDropSystem spawns a Meseta pickup entity instead of crediting the player directly", "[LootDropSystem]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid{4, 4};
    MessageBus bus;
    std::mt19937 rng{1};

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    DropTableComponent table;
    table.meseta_weight = 1.0f;
    table.meseta_min = 10;
    table.meseta_max = 10;
    target.Emplace<DropTableComponent>(table);

    LootDropSystem system(registry, grid, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*is_critical=*/false, /*target_defeated=*/true};
    player.Dispatch(event);

    const std::vector<entt::entity> occupants = grid.GetEntities(Vec2{1, 1});
    REQUIRE(occupants.size() == 2); // target plus the spawned Meseta pickup

    entt::entity dropped = occupants[0] == target.Handle() ? occupants[1] : occupants[0];
    REQUIRE(registry.HasComponent<CurrencyPickupComponent>(dropped));
    CHECK(registry.GetComponent<CurrencyPickupComponent>(dropped).amount == 10);

    // Meseta is credited at pickup time (PickupAction), not here.
    CHECK_FALSE(player.Has<CurrencyComponent>());
}

TEST_CASE("LootDropSystem drops nothing when the roll deterministically favors no_drop_weight", "[LootDropSystem]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid{4, 4};
    MessageBus bus;
    std::mt19937 rng{1};

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    DropTableComponent table;
    table.no_drop_weight = 1.0f; // the only nonzero weight in the pool
    target.Emplace<DropTableComponent>(table);

    LootDropSystem system(registry, grid, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*is_critical=*/false, /*target_defeated=*/true};
    player.Dispatch(event);

    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 1); // nothing spawned
}
