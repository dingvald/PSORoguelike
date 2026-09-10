#include "Items/Storage.h"

#include "Components/InventoryComponent.h"
#include "Components/StorageComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("StoreItem moves an inventory item into storage", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity item = registry.CreateEntity();
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    REQUIRE(psr::StoreItem(actor, 0));

    REQUIRE(actor.Get<psr::InventoryComponent>().items.empty());
    REQUIRE(actor.Get<psr::StorageComponent>().items == std::vector<entt::entity>{item});
}

TEST_CASE("StoreItem is a no-op for an out-of-range index", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::InventoryComponent>();

    REQUIRE_FALSE(psr::StoreItem(actor, 0));
}

TEST_CASE("WithdrawItem moves a storage item back into inventory", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity item = registry.CreateEntity();
    actor.Emplace<psr::StorageComponent>(psr::StorageComponent{{item}});
    actor.Emplace<psr::InventoryComponent>();

    REQUIRE(psr::WithdrawItem(actor, 0));

    REQUIRE(actor.Get<psr::StorageComponent>().items.empty());
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item});
}

TEST_CASE("WithdrawItem is a no-op when the inventory is already at capacity", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity item = registry.CreateEntity();
    actor.Emplace<psr::StorageComponent>(psr::StorageComponent{{item}});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{}, 0});

    REQUIRE_FALSE(psr::WithdrawItem(actor, 0));
    REQUIRE(actor.Get<psr::StorageComponent>().items == std::vector<entt::entity>{item});
}

TEST_CASE("WithdrawItem is a no-op for an out-of-range index", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::StorageComponent>();

    REQUIRE_FALSE(psr::WithdrawItem(actor, 0));
}

TEST_CASE("Storage round-trips an item through store then withdraw", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity item = registry.CreateEntity();
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    REQUIRE(psr::StoreItem(actor, 0));
    REQUIRE(psr::WithdrawItem(actor, 0));

    REQUIRE(actor.Get<psr::StorageComponent>().items.empty());
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item});
}

TEST_CASE("StoreItem tops off an existing matching bank stack instead of opening a second one", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity banked = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(banked, psr::ItemComponent{/*max_stack=*/10, /*quantity=*/4});
    registry.Emplace<psr::PrefabIdComponent>(banked, psr::PrefabIdComponent{5});
    actor.Emplace<psr::StorageComponent>(psr::StorageComponent{{banked}});

    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(item, psr::ItemComponent{/*max_stack=*/10, /*quantity=*/2});
    registry.Emplace<psr::PrefabIdComponent>(item, psr::PrefabIdComponent{5});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    REQUIRE(psr::StoreItem(actor, 0));

    REQUIRE(actor.Get<psr::StorageComponent>().items == std::vector<entt::entity>{banked});
    CHECK(registry.GetComponent<psr::ItemComponent>(banked).quantity == 6);
    CHECK_FALSE(registry.IsValid(item));
}

TEST_CASE("StoreItem opens a second bank stack once the first matching one is full", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity banked = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(banked, psr::ItemComponent{/*max_stack=*/5, /*quantity=*/5});
    registry.Emplace<psr::PrefabIdComponent>(banked, psr::PrefabIdComponent{5});
    actor.Emplace<psr::StorageComponent>(psr::StorageComponent{{banked}});

    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(item, psr::ItemComponent{/*max_stack=*/5, /*quantity=*/3});
    registry.Emplace<psr::PrefabIdComponent>(item, psr::PrefabIdComponent{5});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    REQUIRE(psr::StoreItem(actor, 0));

    REQUIRE(actor.Get<psr::StorageComponent>().items == std::vector<entt::entity>{banked, item});
    CHECK(registry.GetComponent<psr::ItemComponent>(banked).quantity == 5);
    CHECK(registry.GetComponent<psr::ItemComponent>(item).quantity == 3);
}

TEST_CASE("WithdrawItem tops off an existing matching inventory stack and leaves the remainder banked",
          "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity held = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(held, psr::ItemComponent{/*max_stack=*/10, /*quantity=*/8});
    registry.Emplace<psr::PrefabIdComponent>(held, psr::PrefabIdComponent{5});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{held}, 20});

    entt::entity banked = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(banked, psr::ItemComponent{/*max_stack=*/10, /*quantity=*/5});
    registry.Emplace<psr::PrefabIdComponent>(banked, psr::PrefabIdComponent{5});
    actor.Emplace<psr::StorageComponent>(psr::StorageComponent{{banked}});

    REQUIRE(psr::WithdrawItem(actor, 0));

    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{held});
    CHECK(registry.GetComponent<psr::ItemComponent>(held).quantity == 10);
    REQUIRE(actor.Get<psr::StorageComponent>().items == std::vector<entt::entity>{banked});
    CHECK(registry.GetComponent<psr::ItemComponent>(banked).quantity == 3);
}

TEST_CASE("WithdrawItem is a no-op when the matching inventory stack is already full", "[Storage]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity held = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(held, psr::ItemComponent{/*max_stack=*/10, /*quantity=*/10});
    registry.Emplace<psr::PrefabIdComponent>(held, psr::PrefabIdComponent{5});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{held}, 20});

    entt::entity banked = registry.CreateEntity();
    registry.Emplace<psr::ItemComponent>(banked, psr::ItemComponent{/*max_stack=*/10, /*quantity=*/5});
    registry.Emplace<psr::PrefabIdComponent>(banked, psr::PrefabIdComponent{5});
    actor.Emplace<psr::StorageComponent>(psr::StorageComponent{{banked}});

    REQUIRE_FALSE(psr::WithdrawItem(actor, 0));

    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{held});
    REQUIRE(actor.Get<psr::StorageComponent>().items == std::vector<entt::entity>{banked});
    CHECK(registry.GetComponent<psr::ItemComponent>(banked).quantity == 5);
}
