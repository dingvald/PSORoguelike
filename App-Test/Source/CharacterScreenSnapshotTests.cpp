#include "Items/CharacterScreenSnapshot.h"

#include "Components/ConsumableComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/AffixLibrary.h"
#include "Items/Equip.h"
#include "Messages/CharacterScreenMessage.h"

#include <catch2/catch_test_macros.hpp>

namespace {
psr::AffixLibrary g_no_affixes;
} // namespace

TEST_CASE("BuildCharacterScreenMessage tags inventory entries with their equip_slot/is_consumable", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);

    entt::entity armor = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(armor, psr::ArmorComponent{psr::ArmorSlot::Legs, 0});

    entt::entity potion = registry.CreateEntity();
    registry.Emplace<psr::ConsumableComponent>(potion, psr::ConsumableComponent{psr::ConsumableEffect::RestoreHp, 10});

    entt::entity trinket = registry.CreateEntity(); // neither weapon, armor, nor consumable

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{weapon, armor, potion, trinket}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes);

    REQUIRE(message.inventory.size() == 4);

    REQUIRE(message.inventory[0].equip_slot == psr::EquipmentSlot::Weapon);
    REQUIRE_FALSE(message.inventory[0].is_consumable);

    REQUIRE(message.inventory[1].equip_slot == psr::EquipmentSlot::Legs);
    REQUIRE_FALSE(message.inventory[1].is_consumable);

    REQUIRE_FALSE(message.inventory[2].equip_slot.has_value());
    REQUIRE(message.inventory[2].is_consumable);

    REQUIRE_FALSE(message.inventory[3].equip_slot.has_value());
    REQUIRE_FALSE(message.inventory[3].is_consumable);
}

TEST_CASE("BuildCharacterScreenMessage fills mod_slot_labels from ArmorComponent::mod_slot_count",
          "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity armor = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(armor, psr::ArmorComponent{psr::ArmorSlot::Torso, 2});

    entt::entity no_slots_armor = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(no_slots_armor, psr::ArmorComponent{psr::ArmorSlot::Head, 0});

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{armor, no_slots_armor}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes);

    REQUIRE(message.inventory[0].mod_slot_labels.size() == 2);
    REQUIRE(message.inventory[0].mod_slot_labels[0] == "(empty)");
    REQUIRE(message.inventory[0].mod_slot_labels[1] == "(empty)");

    REQUIRE(message.inventory[1].mod_slot_labels.empty());
}

TEST_CASE("BuildCharacterScreenMessage resolves equipment slot entries the same way", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    registry.Emplace<psr::EquipmentComponent>(player, psr::EquipmentComponent{weapon});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes);

    REQUIRE(message.equipment[static_cast<std::size_t>(psr::EquipmentSlot::Weapon)].has_value());
    REQUIRE(message.equipment[static_cast<std::size_t>(psr::EquipmentSlot::Weapon)]->equip_slot ==
            psr::EquipmentSlot::Weapon);
    REQUIRE_FALSE(message.equipment[static_cast<std::size_t>(psr::EquipmentSlot::Head)].has_value());
}

TEST_CASE("BuildCharacterScreenMessage populates HP/TP and effective stats", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    registry.Emplace<psr::HealthComponent>(player, psr::HealthComponent{30, 50});
    registry.Emplace<psr::TPComponent>(player, psr::TPComponent{5, 20});

    psr::StatsComponent base;
    base.atp = 10;
    base.lck = 3;
    registry.Emplace<psr::StatsComponent>(player, base);

    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent weapon_component;
    weapon_component.grind_level = 2; // +2 ATP per grind level, per EffectiveStats.cpp
    registry.Emplace<psr::WeaponComponent>(weapon, weapon_component);
    registry.Emplace<psr::EquipmentComponent>(player, psr::EquipmentComponent{weapon});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes);

    REQUIRE(message.stats.hp == 30);
    REQUIRE(message.stats.max_hp == 50);
    REQUIRE(message.stats.tp == 5);
    REQUIRE(message.stats.max_tp == 20);
    REQUIRE(message.stats.atp == 14); // base 10 + grind 2*2
    REQUIRE(message.stats.lck == 3);
}
