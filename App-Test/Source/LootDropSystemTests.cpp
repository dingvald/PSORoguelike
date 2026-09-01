#include "Systems/LootDropSystem.h"

#include "Components/CurrencyComponent.h"
#include "Components/DropTableComponent.h"
#include "Components/SectionIdComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"
#include "Engine/World/Grid.h"
#include "Items/DropTable.h"
#include "Items/DropTableLibrary.h"
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

} // namespace

TEST_CASE("LootDropSystem no-ops when the hit did not defeat the target", "[LootDropSystem]")
{
    Registry registry;
    ItemMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid{4, 4};
    MessageBus bus;
    std::mt19937 rng{1};
    DropTableLibrary drop_tables;

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<DropTableComponent>(DropTableComponent{kItemPrefab});

    LootDropSystem system(registry, grid, drop_tables, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/5, /*target_defeated=*/false};
    player.Dispatch(event);

    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 1); // only target itself, nothing dropped
}

TEST_CASE("LootDropSystem no-ops when the defeated target has no DropTableComponent", "[LootDropSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    std::mt19937 rng{1};
    DropTableLibrary drop_tables;

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});

    LootDropSystem system(registry, grid, drop_tables, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/5, /*target_defeated=*/true};
    player.Dispatch(event);

    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 1); // no drop-table ref -- nothing dropped
    CHECK_FALSE(player.Has<CurrencyComponent>());
}

TEST_CASE("LootDropSystem spawns a ground item and credits Meseta on a lethal player hit", "[LootDropSystem]")
{
    Registry registry;
    ItemMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid{4, 4};
    MessageBus bus;
    MessageQueue hud_queue;
    std::mt19937 rng{1};

    DropTable table;
    table.id = entt::hashed_string::value("booma");
    table.guaranteed_item_ids = {kItemPrefab};
    table.meseta_min = 10;
    table.meseta_max = 10;
    DropTableLibrary drop_tables{std::vector<DropTable>{table}};

    std::vector<std::string> loot_names;
    hud_queue.RegisterHandler<LootDropMessage>([&](const LootDropMessage& m) { loot_names.push_back(m.item_name); });
    int meseta_updates = 0;
    int last_meseta = -1;
    hud_queue.RegisterHandler<MesetaChangedMessage>(
        [&](const MesetaChangedMessage& m)
        {
            ++meseta_updates;
            last_meseta = m.current_meseta;
        });
    bus.Subscribe<LootDropMessage>(hud_queue);
    bus.Subscribe<MesetaChangedMessage>(hud_queue);

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<DropTableComponent>(DropTableComponent{table.id});

    LootDropSystem system(registry, grid, drop_tables, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*target_defeated=*/true};
    player.Dispatch(event);

    // The pre-existing target entity plus one freshly-spawned item.
    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 2);

    REQUIRE(player.Has<CurrencyComponent>());
    CHECK(player.Get<CurrencyComponent>().meseta == 10);

    hud_queue.HandleQueuedMessages();
    CHECK(loot_names.size() == 1);
    CHECK(meseta_updates == 1);
    CHECK(last_meseta == 10);
}

TEST_CASE("LootDropSystem uses the player's SectionIdComponent to weight the roll", "[LootDropSystem]")
{
    Registry registry;
    ItemMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    Grid grid{4, 4};
    MessageBus bus;
    std::mt19937 rng{1};

    DropTable table;
    table.id = entt::hashed_string::value("booma");
    // No entries at all -- with a favored Section ID contributing zero weight
    // to the only entry, this deterministically confirms the roll runs
    // without the entry (rather than asserting which of two entries wins,
    // covered already by DropTableRollerTests).
    DropTableEntry entry;
    entry.item_prefab_id = kItemPrefab;
    entry.section_id_weights[static_cast<std::size_t>(SectionId::Redria)] = 0.0f;
    table.common_entries = {entry};
    DropTableLibrary drop_tables{std::vector<DropTable>{table}};

    Entity player = MakeActorAt(registry, grid, {0, 0});
    player.Emplace<SectionIdComponent>(SectionIdComponent{SectionId::Redria});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<DropTableComponent>(DropTableComponent{table.id});

    LootDropSystem system(registry, grid, drop_tables, bus, rng);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*target_defeated=*/true};
    player.Dispatch(event);

    // Redria zeroes the only entry's weight -- nothing should have dropped.
    CHECK(grid.GetEntities(Vec2{1, 1}).size() == 1);
}
