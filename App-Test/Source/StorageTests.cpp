#include "Items/Storage.h"

#include "Components/InventoryComponent.h"
#include "Components/StorageComponent.h"
#include "Engine/ECS/Entity.h"
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
