#include "Actions/PickupAction.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/CurrencyComponent.h"
#include "Components/CurrencyPickupComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemPickupEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"
#include "Engine/World/Grid.h"
#include "Messages/MesetaChangedMessage.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

namespace {

entt::entity MakeGroundItem(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, std::uint32_t prefab_id = 0)
{
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(item);
    registry.Emplace<psr::Position>(item, psr::Position{tile});
    if (prefab_id != 0)
        registry.Emplace<psr::PrefabIdComponent>(item, psr::PrefabIdComponent{prefab_id});
    grid.AddEntity(tile, item);
    return item;
}

entt::entity MakeGroundCurrencyPickup(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, int amount)
{
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(item);
    registry.Emplace<psr::CurrencyPickupComponent>(item, psr::CurrencyPickupComponent{amount});
    registry.Emplace<psr::Position>(item, psr::Position{tile});
    grid.AddEntity(tile, item);
    return item;
}

} // namespace

TEST_CASE("PickupAction is a free no-op when the actor's tile has no items", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::MessageBus bus;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    psr::PickupAction action(grid, bus);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE_FALSE(actor.Has<psr::InventoryComponent>());
}

TEST_CASE("PickupAction moves a single item into the actor's inventory and off the Grid", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::MessageBus bus;
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

    psr::PickupAction action(grid, bus);
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
    psr::MessageBus bus;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity item_a = MakeGroundItem(registry, grid, psr::Vec2{1, 1});
    entt::entity item_b = MakeGroundItem(registry, grid, psr::Vec2{1, 1});

    psr::PickupAction action(grid, bus);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PickupAction::kPickupCost);
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item_a, item_b});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle});
}

TEST_CASE("PickupAction ignores occupants without ItemComponent", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::MessageBus bus;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity decoration = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(decoration);
    grid.AddEntity(psr::Vec2{1, 1}, decoration);

    psr::PickupAction action(grid, bus);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE_FALSE(actor.Has<psr::InventoryComponent>());
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle, decoration});
}

TEST_CASE("PickupAction leaves items on the ground once the actor's inventory is at capacity", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::MessageBus bus;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{}, /*capacity=*/1});

    entt::entity item_a = MakeGroundItem(registry, grid, psr::Vec2{1, 1});
    entt::entity item_b = MakeGroundItem(registry, grid, psr::Vec2{1, 1});

    psr::PickupAction action(grid, bus);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PickupAction::kPickupCost);
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item_a});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle, item_b});
}

TEST_CASE("PickupAction credits a Meseta pickup directly, never entering InventoryComponent", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    int meseta_updates = 0;
    int last_current = -1;
    int last_delta = -1;
    hud_queue.RegisterHandler<psr::MesetaChangedMessage>(
        [&](const psr::MesetaChangedMessage& m)
        {
            ++meseta_updates;
            last_current = m.current_meseta;
            last_delta = m.delta;
        });
    bus.Subscribe<psr::MesetaChangedMessage>(hud_queue);

    entt::entity pile = MakeGroundCurrencyPickup(registry, grid, psr::Vec2{1, 1}, /*amount=*/25);

    psr::PickupAction action(grid, bus);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PickupAction::kPickupCost);
    REQUIRE(actor.Get<psr::CurrencyComponent>().meseta == 25);
    REQUIRE_FALSE(registry.IsValid(pile));
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle});
    if (actor.Has<psr::InventoryComponent>())
        REQUIRE(actor.Get<psr::InventoryComponent>().items.empty());

    hud_queue.HandleQueuedMessages();
    CHECK(meseta_updates == 1);
    CHECK(last_current == 25);
    CHECK(last_delta == 25);
}

TEST_CASE("PickupAction credits a Meseta pickup even when the actor's inventory is already full", "[PickupAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::MessageBus bus;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{}, /*capacity=*/0});

    MakeGroundCurrencyPickup(registry, grid, psr::Vec2{1, 1}, /*amount=*/5);

    psr::PickupAction action(grid, bus);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PickupAction::kPickupCost);
    REQUIRE(actor.Get<psr::CurrencyComponent>().meseta == 5);
    REQUIRE(actor.Get<psr::InventoryComponent>().items.empty());
}
