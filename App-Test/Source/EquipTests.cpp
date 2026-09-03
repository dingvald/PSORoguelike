#include "Items/Equip.h"

#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("EquipItem moves a weapon from inventory into the weapon slot", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{weapon}, 20});

    REQUIRE(psr::EquipItem(actor, 0));

    REQUIRE(actor.Get<psr::EquipmentComponent>().weapon == weapon);
    REQUIRE(actor.Get<psr::InventoryComponent>().items.empty());
}

TEST_CASE("EquipItem swaps the previously equipped weapon back into inventory", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity old_weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(old_weapon);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{old_weapon});

    entt::entity new_weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(new_weapon);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{new_weapon}, 20});

    REQUIRE(psr::EquipItem(actor, 0));

    REQUIRE(actor.Get<psr::EquipmentComponent>().weapon == new_weapon);
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{old_weapon});
}

TEST_CASE("EquipItem routes armor to the EquipmentComponent slot matching its ArmorSlot", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity helmet = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(helmet, psr::ArmorComponent{psr::ArmorSlot::Head, 0});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{helmet}, 20});

    REQUIRE(psr::EquipItem(actor, 0));
    REQUIRE(actor.Get<psr::EquipmentComponent>().head == helmet);
}

TEST_CASE("EquipItem is a no-op for an item with no equip slot", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity mod = registry.CreateEntity(); // no WeaponComponent/ArmorComponent
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{mod}, 20});

    REQUIRE_FALSE(psr::EquipItem(actor, 0));
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{mod});
}

TEST_CASE("EquipItem is a no-op for an out-of-range index", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::InventoryComponent>();

    REQUIRE_FALSE(psr::EquipItem(actor, 0));
}

TEST_CASE("UnequipSlot is a no-op for an empty slot", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::EquipmentComponent>();

    REQUIRE_FALSE(psr::UnequipSlot(actor, psr::EquipmentSlot::Weapon));
}

TEST_CASE("UnequipSlot is a no-op when the inventory is already at capacity", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{}, 0});

    REQUIRE_FALSE(psr::UnequipSlot(actor, psr::EquipmentSlot::Weapon));
    REQUIRE(actor.Get<psr::EquipmentComponent>().weapon == weapon);
}

TEST_CASE("UnequipSlot moves the item back into inventory and clears the slot", "[Equip]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    REQUIRE(psr::UnequipSlot(actor, psr::EquipmentSlot::Weapon));

    REQUIRE((actor.Get<psr::EquipmentComponent>().weapon == entt::null));
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{weapon});
}
