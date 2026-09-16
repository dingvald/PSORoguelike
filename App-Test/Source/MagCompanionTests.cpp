#include "Items/Mag/MagCompanion.h"

#include "Components/EquipmentComponent.h"
#include "Components/LastDirectionComponent.h"
#include "Components/MagComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("OnMagEquipped places the mag one tile behind the actor, opposite its last direction", "[MagCompanion]")
{
    psr::Registry registry;
    psr::Grid grid(10, 10);
    registry.SetGrid(grid);

    entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{{5, 5}});
    registry.Emplace<psr::LastDirectionComponent>(actor, psr::LastDirectionComponent{{1, 0}}); // last moved east

    entt::entity mag = registry.CreateEntity();
    registry.Emplace<psr::MagComponent>(mag);

    psr::OnMagEquipped(registry, actor, mag);

    const psr::Position& mag_position = registry.GetComponent<psr::Position>(mag);
    CHECK((mag_position.tile == psr::Vec2{4, 5})); // opposite of east is west
    CHECK(grid.GetEntities({4, 5}).size() == 1);
    CHECK(grid.GetEntities({4, 5})[0] == mag);
}

TEST_CASE("OnMagEquipped is a no-op when the actor has no Position", "[MagCompanion]")
{
    psr::Registry registry;
    psr::Grid grid(10, 10);
    registry.SetGrid(grid);

    entt::entity actor = registry.CreateEntity();
    entt::entity mag = registry.CreateEntity();
    registry.Emplace<psr::MagComponent>(mag);

    psr::OnMagEquipped(registry, actor, mag);

    CHECK_FALSE(registry.HasComponent<psr::Position>(mag));
}

TEST_CASE("OnMagUnequipped removes the mag from the Grid and clears its Position", "[MagCompanion]")
{
    psr::Registry registry;
    psr::Grid grid(10, 10);
    registry.SetGrid(grid);

    entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{{5, 5}});
    registry.Emplace<psr::LastDirectionComponent>(actor);

    entt::entity mag = registry.CreateEntity();
    registry.Emplace<psr::MagComponent>(mag);
    psr::OnMagEquipped(registry, actor, mag);

    psr::OnMagUnequipped(registry, mag);

    CHECK_FALSE(registry.HasComponent<psr::Position>(mag));
    CHECK(grid.GetEntities({4, 5}).empty());
}

TEST_CASE("UpdateMagCompanion repositions the equipped mag as the actor's last direction changes", "[MagCompanion]")
{
    psr::Registry registry;
    psr::Grid grid(10, 10);
    registry.SetGrid(grid);

    entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{{5, 5}});
    registry.Emplace<psr::LastDirectionComponent>(actor, psr::LastDirectionComponent{{0, 1}}); // facing south

    entt::entity mag = registry.CreateEntity();
    registry.Emplace<psr::MagComponent>(mag);
    registry.Emplace<psr::EquipmentComponent>(actor, psr::EquipmentComponent{entt::null, entt::null, entt::null,
                                                                              entt::null, entt::null, mag});
    psr::OnMagEquipped(registry, actor, mag);
    REQUIRE((registry.GetComponent<psr::Position>(mag).tile == psr::Vec2{5, 4})); // opposite south is north

    registry.GetComponent<psr::LastDirectionComponent>(actor).direction = {1, 0}; // now facing east
    psr::UpdateMagCompanion(registry, actor, /*delta_time=*/0.1f);

    const psr::Position& mag_position = registry.GetComponent<psr::Position>(mag);
    CHECK((mag_position.tile == psr::Vec2{4, 5})); // opposite east is west
    CHECK(grid.GetEntities({5, 4}).empty());        // vacated the old trailing tile
    CHECK(grid.GetEntities({4, 5}).size() == 1);
    CHECK(registry.GetComponent<psr::MagComponent>(mag).bob_elapsed == 0.1f);
}

TEST_CASE("UpdateMagCompanion is a no-op when the actor has no mag equipped", "[MagCompanion]")
{
    psr::Registry registry;
    psr::Grid grid(10, 10);
    registry.SetGrid(grid);

    entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{{5, 5}});
    registry.Emplace<psr::EquipmentComponent>(actor);

    psr::UpdateMagCompanion(registry, actor, 0.1f); // should not throw/assert with an empty mag slot
    SUCCEED();
}
