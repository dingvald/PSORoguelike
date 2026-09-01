#include "Actions/PickupAction.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemPickupEvent.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

namespace {

entt::entity MakeGroundItem(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile,
                            std::uint32_t prefab_id = 0)
{
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(item);
    registry.Emplace<psr::Position>(item, psr::Position{tile});
    if (prefab_id != 0)
        registry.Emplace<psr::PrefabIdComponent>(item, psr::PrefabIdComponent{prefab_id});
    grid.AddEntity(tile, item);
    return item;
}

} // namespace

TEST_CASE("PickupAction is a free no-op when the actor's tile has no items", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    psr::PickupAction action(grid);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE_FALSE(actor.Has<psr::InventoryComponent>());
}

TEST_CASE("PickupAction moves a single item into the actor's inventory and off the Grid", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity item = MakeGroundItem(registry, grid, psr::Vec2{1, 1}, /*prefab_id=*/42);

    struct PickupProbe
    {
    };
    std::optional<psr::AfterItemPickupEvent> received;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterItemPickupEvent, PickupProbe>(
        [&](psr::Entity, psr::AfterItemPickupEvent& event) { received = event; });

    psr::PickupAction action(grid);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PickupAction::kPickupCost);
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle});
    REQUIRE(received.has_value());
    REQUIRE(received->item_prefab_id == 42);
}

TEST_CASE("PickupAction picks up every item sharing the actor's tile in one turn", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity item_a = MakeGroundItem(registry, grid, psr::Vec2{1, 1});
    entt::entity item_b = MakeGroundItem(registry, grid, psr::Vec2{1, 1});

    psr::PickupAction action(grid);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PickupAction::kPickupCost);
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item_a, item_b});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle});
}

TEST_CASE("PickupAction ignores occupants without ItemComponent", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity decoration = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(decoration);
    grid.AddEntity(psr::Vec2{1, 1}, decoration);

    psr::PickupAction action(grid);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE_FALSE(actor.Has<psr::InventoryComponent>());
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle, decoration});
}

TEST_CASE("PickupAction leaves items on the ground once the actor's inventory is at capacity", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{}, /*capacity=*/1});

    entt::entity item_a = MakeGroundItem(registry, grid, psr::Vec2{1, 1});
    entt::entity item_b = MakeGroundItem(registry, grid, psr::Vec2{1, 1});

    psr::PickupAction action(grid);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PickupAction::kPickupCost);
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item_a});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle, item_b});
}
