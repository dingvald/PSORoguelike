#include "Items/StorageSnapshot.h"

#include "Components/InventoryComponent.h"
#include "Components/StorageComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/AffixLibrary.h"
#include "Items/Equip.h"

#include <catch2/catch_test_macros.hpp>

namespace {
psr::AffixLibrary g_no_affixes;
} // namespace

TEST_CASE("BuildStorageMessage resolves both the player's inventory and storage lists", "[StorageSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{weapon}, 20});

    entt::entity armor = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(armor, psr::ArmorComponent{psr::ArmorSlot::Head, 0});
    registry.Emplace<psr::StorageComponent>(player, psr::StorageComponent{{armor}});

    const psr::StorageMessage message = psr::BuildStorageMessage(registry, player, g_no_affixes);

    REQUIRE(message.inventory.size() == 1);
    REQUIRE(message.inventory[0].equip_slot == psr::EquipmentSlot::Weapon);

    REQUIRE(message.storage.size() == 1);
    REQUIRE(message.storage[0].equip_slot == psr::EquipmentSlot::Head);
}

TEST_CASE("BuildStorageMessage is empty for a player with no inventory/storage components", "[StorageSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    const psr::StorageMessage message = psr::BuildStorageMessage(registry, player, g_no_affixes);

    REQUIRE(message.inventory.empty());
    REQUIRE(message.storage.empty());
}
