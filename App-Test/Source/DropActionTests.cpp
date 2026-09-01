#include "Actions/DropAction.h"

#include "Components/InventoryComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemDropEvent.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

TEST_CASE("DropAction is a free no-op when the actor has no InventoryComponent", "[DropAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    psr::DropAction action(grid, /*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("DropAction is a free no-op for an out-of-range index", "[DropAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);
    actor.Emplace<psr::InventoryComponent>();

    psr::DropAction action(grid, /*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("DropAction removes the item at the given index and places it back on the Grid", "[DropAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::Position>(item, psr::Position{psr::Vec2{0, 0}}); // stale, from wherever it was picked up
    registry.Emplace<psr::PrefabIdComponent>(item, psr::PrefabIdComponent{7});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    struct DropProbe
    {
    };
    std::optional<psr::AfterItemDropEvent> received;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterItemDropEvent, DropProbe>(
        [&](psr::Entity, psr::AfterItemDropEvent& event) { received = event; });

    psr::DropAction action(grid, /*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::DropAction::kDropCost);
    REQUIRE(actor.Get<psr::InventoryComponent>().items.empty());
    REQUIRE(registry.GetComponent<psr::Position>(item).tile == psr::Vec2{1, 1});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle, item});
    REQUIRE(received.has_value());
    REQUIRE(received->item_prefab_id == 7);
}
