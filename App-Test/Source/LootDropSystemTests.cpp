#include "Systems/LootDropSystem.h"

#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/DropTableComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/MesetaComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/SectionIdComponent.h"
#include "Engine/Items/DropTableLibrary.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::uint32_t kTableId = 1;

psr::DropTableLibrary MakeLibraryWithMeseta(int meseta_min, int meseta_max)
{
    psr::DropTable table;
    table.id = kTableId;
    table.id_string = "test_table";
    table.meseta_min = meseta_min;
    table.meseta_max = meseta_max;
    return psr::DropTableLibrary{std::vector<psr::DropTable>{table}};
}

entt::entity MakePlayer(psr::Registry& registry)
{
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::MesetaComponent>(player);
    registry.Emplace<psr::SectionIdComponent>(player);
    return player;
}

} // namespace

TEST_CASE("LootDropSystem credits Meseta to the player on a dying entity with a matching DropTableComponent",
         "[LootDropSystem]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::DropTableLibrary drop_tables = MakeLibraryWithMeseta(/*meseta_min=*/10, /*meseta_max=*/10);
    std::mt19937 rng{1};

    const entt::entity player = MakePlayer(registry);
    psr::LootDropSystem loot_drop_system(registry, grid, drop_tables, rng);

    entt::entity enemy = registry.CreateEntity();
    registry.Emplace<psr::Position>(enemy, psr::Position{psr::Vec2{2, 2}});
    registry.Emplace<psr::HealthComponent>(enemy, psr::HealthComponent{/*current_hp=*/0, /*max_hp=*/10});
    registry.Emplace<psr::DropTableComponent>(enemy, psr::DropTableComponent{kTableId});

    registry.DestroyEntity(enemy);

    CHECK(registry.GetComponent<psr::MesetaComponent>(player).amount == 10);
}

TEST_CASE("LootDropSystem is a no-op for an entity with no DropTableComponent", "[LootDropSystem]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::DropTableLibrary drop_tables = MakeLibraryWithMeseta(/*meseta_min=*/10, /*meseta_max=*/10);
    std::mt19937 rng{2};

    const entt::entity player = MakePlayer(registry);
    psr::LootDropSystem loot_drop_system(registry, grid, drop_tables, rng);

    entt::entity enemy = registry.CreateEntity();
    registry.Emplace<psr::Position>(enemy, psr::Position{psr::Vec2{2, 2}});
    registry.Emplace<psr::HealthComponent>(enemy, psr::HealthComponent{/*current_hp=*/0, /*max_hp=*/10});
    // No DropTableComponent.

    registry.DestroyEntity(enemy);

    CHECK(registry.GetComponent<psr::MesetaComponent>(player).amount == 0);
}

TEST_CASE("LootDropSystem is a no-op for an entity whose DropTableComponent references an unknown table",
         "[LootDropSystem]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::DropTableLibrary drop_tables = MakeLibraryWithMeseta(/*meseta_min=*/10, /*meseta_max=*/10);
    std::mt19937 rng{3};

    const entt::entity player = MakePlayer(registry);
    psr::LootDropSystem loot_drop_system(registry, grid, drop_tables, rng);

    entt::entity enemy = registry.CreateEntity();
    registry.Emplace<psr::Position>(enemy, psr::Position{psr::Vec2{2, 2}});
    registry.Emplace<psr::HealthComponent>(enemy, psr::HealthComponent{/*current_hp=*/0, /*max_hp=*/10});
    registry.Emplace<psr::DropTableComponent>(enemy, psr::DropTableComponent{/*drop_table_id=*/999});

    registry.DestroyEntity(enemy);

    CHECK(registry.GetComponent<psr::MesetaComponent>(player).amount == 0);
}

TEST_CASE("LootDropSystem does not roll loot for an entity destroyed without a HealthComponent", "[LootDropSystem]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::DropTableLibrary drop_tables = MakeLibraryWithMeseta(/*meseta_min=*/10, /*meseta_max=*/10);
    std::mt19937 rng{4};

    const entt::entity player = MakePlayer(registry);
    psr::LootDropSystem loot_drop_system(registry, grid, drop_tables, rng);

    // A DropTableComponent alone, with no HealthComponent, should not roll
    // loot -- only combat deaths do (see LootDropSystem's own guard).
    entt::entity non_combat_entity = registry.CreateEntity();
    registry.Emplace<psr::DropTableComponent>(non_combat_entity, psr::DropTableComponent{kTableId});

    registry.DestroyEntity(non_combat_entity);

    CHECK(registry.GetComponent<psr::MesetaComponent>(player).amount == 0);
}
