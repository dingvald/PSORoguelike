#include "Items/CharacterScreenSnapshot.h"

#include "Combat/Element.h"
#include "Combat/PhotonArt.h"
#include "Combat/PhotonArtLibrary.h"
#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectLibrary.h"
#include "Components/ConsumableComponent.h"
#include "Components/CurrencyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/LevelComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/RarityComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/AffixLibrary.h"
#include "Items/Equip.h"
#include "Messages/CharacterScreenMessage.h"
#include "Progression/GrowthCurve.h"

#include <catch2/catch_test_macros.hpp>

namespace {
psr::AffixLibrary g_no_affixes;
psr::GrowthCurve g_no_growth_curve;
psr::PhotonArtLibrary g_no_photon_arts;
psr::StatusEffectLibrary g_no_status_effects;
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

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

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

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

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

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

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

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.stats.hp == 30);
    REQUIRE(message.stats.max_hp == 50);
    REQUIRE(message.stats.tp == 5);
    REQUIRE(message.stats.max_tp == 20);
    REQUIRE(message.stats.atp == 14); // base 10 + grind 2*2
    REQUIRE(message.stats.lck == 3);
}

TEST_CASE("BuildCharacterScreenMessage populates meseta and level/xp fields", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    registry.Emplace<psr::CurrencyComponent>(player, psr::CurrencyComponent{500});
    registry.Emplace<psr::LevelComponent>(player, psr::LevelComponent{3, 40, 340});

    psr::GrowthCurve growth_curve;
    growth_curve.xp_to_next = {.base = 100.0f};

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.meseta == 500);
    REQUIRE(message.stats.level == 3);
    REQUIRE(message.stats.xp == 40);
    REQUIRE(message.stats.xp_to_next == 100);
    REQUIRE(message.stats.total_xp == 340);
}

TEST_CASE("BuildCharacterScreenMessage's xp_to_next is 0 for an empty (all-zero) growth curve", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    registry.Emplace<psr::LevelComponent>(player, psr::LevelComponent{99, 0, 12345});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.stats.xp_to_next == 0);
}

TEST_CASE("BuildCharacterScreenMessage populates rarity_stars and description", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity armor = registry.CreateEntity();
    registry.Emplace<psr::RarityComponent>(armor, psr::RarityComponent{4});
    registry.Emplace<psr::ItemComponent>(armor, psr::ItemComponent{1, 1, "A sturdy frame."});

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{armor}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.inventory[0].rarity_stars == 4);
    REQUIRE(message.inventory[0].description == "A sturdy frame.");
}

TEST_CASE("BuildCharacterScreenMessage populates weapon_detail's range/targeting/grind fields", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent weapon_component;
    weapon_component.range_shape = psr::WeaponRangeShape::Line;
    weapon_component.range = 5;
    weapon_component.hits_per_turn = 2;
    weapon_component.grind_level = 3;
    weapon_component.max_grind_level = 8;
    weapon_component.fires_projectile = true;
    weapon_component.targeting_mode = psr::TargetingMode::TargetSquare;
    registry.Emplace<psr::WeaponComponent>(weapon, weapon_component);

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{weapon}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.inventory[0].weapon_detail.has_value());
    const auto& detail = *message.inventory[0].weapon_detail;
    REQUIRE(detail.range_shape == psr::WeaponRangeShape::Line);
    REQUIRE(detail.range == 5);
    REQUIRE(detail.hits_per_turn == 2);
    REQUIRE(detail.grind_level == 3);
    REQUIRE(detail.max_grind_level == 8);
    REQUIRE(detail.fires_projectile);
    REQUIRE(detail.targeting_mode == psr::TargetingMode::TargetSquare);
    REQUIRE(detail.status_effect_name.empty());
    REQUIRE(detail.photon_art_names.empty());
}

TEST_CASE("BuildCharacterScreenMessage resolves weapon_detail's status effect name only when elemental",
          "[CharacterScreenSnapshot]")
{
    psr::StatusEffectLibrary status_effects({psr::StatusEffect{1, "poison_weak", "Weak Poison"}});

    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity fire_weapon = registry.CreateEntity();
    psr::WeaponComponent fire_weapon_component;
    fire_weapon_component.element = psr::Element::Fire;
    fire_weapon_component.status_effect_id = 1;
    fire_weapon_component.status_chance_percent = 30;
    registry.Emplace<psr::WeaponComponent>(fire_weapon, fire_weapon_component);

    entt::entity plain_weapon = registry.CreateEntity();
    psr::WeaponComponent plain_weapon_component;
    plain_weapon_component.status_effect_id = 1;
    plain_weapon_component.status_chance_percent = 30;
    registry.Emplace<psr::WeaponComponent>(plain_weapon, plain_weapon_component);

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{fire_weapon, plain_weapon}, 20});

    const psr::CharacterScreenMessage message =
        psr::BuildCharacterScreenMessage(registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, status_effects);

    REQUIRE(message.inventory[0].weapon_detail->status_effect_name == "Weak Poison");
    REQUIRE(message.inventory[0].weapon_detail->status_chance_percent == 30);
    REQUIRE(message.inventory[1].weapon_detail->status_effect_name.empty());
}

TEST_CASE("BuildCharacterScreenMessage resolves weapon_detail's photon_art_names from photon_art_ids",
          "[CharacterScreenSnapshot]")
{
    psr::PhotonArt art;
    art.id = 7;
    art.id_string = "rising_strike";
    art.name = "Rising Strike";
    psr::PhotonArtLibrary photon_arts({art});

    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent weapon_component;
    weapon_component.photon_art_ids = {7};
    registry.Emplace<psr::WeaponComponent>(weapon, weapon_component);

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{weapon}, 20});

    const psr::CharacterScreenMessage message =
        psr::BuildCharacterScreenMessage(registry, player, g_no_affixes, g_no_growth_curve, photon_arts, g_no_status_effects);

    REQUIRE(message.inventory[0].weapon_detail->photon_art_names == std::vector<std::string>{"Rising Strike"});
}

TEST_CASE("BuildCharacterScreenMessage flags a weapon's unmet stat_requirement", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::StatsComponent>(player, psr::StatsComponent{50, 0, 0, 0, 0, 0}); // 50 ATP

    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent weapon_component;
    weapon_component.stat_requirement = {psr::AffixStat::Atp, 100};
    registry.Emplace<psr::WeaponComponent>(weapon, weapon_component);

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{weapon}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.inventory[0].requirement_text == "Requires 100 ATP");
    REQUIRE_FALSE(message.inventory[0].requirement_met);
}

TEST_CASE("BuildCharacterScreenMessage marks a weapon's stat_requirement met when the player qualifies",
          "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::StatsComponent>(player, psr::StatsComponent{150, 0, 0, 0, 0, 0}); // 150 ATP

    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent weapon_component;
    weapon_component.stat_requirement = {psr::AffixStat::Atp, 100};
    registry.Emplace<psr::WeaponComponent>(weapon, weapon_component);

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{weapon}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.inventory[0].requirement_text == "Requires 100 ATP");
    REQUIRE(message.inventory[0].requirement_met);
}

TEST_CASE("BuildCharacterScreenMessage flags an armor's unmet required_level", "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::LevelComponent>(player, psr::LevelComponent{3, 0, 0});

    entt::entity armor = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(armor, psr::ArmorComponent{psr::ArmorSlot::Torso, 0, 10});

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{armor}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.inventory[0].requirement_text == "Requires Level 10");
    REQUIRE_FALSE(message.inventory[0].requirement_met);
}

TEST_CASE("BuildCharacterScreenMessage leaves requirement_text empty and requirement_met true with no requirement",
          "[CharacterScreenSnapshot]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);

    registry.Emplace<psr::InventoryComponent>(player, psr::InventoryComponent{{weapon}, 20});

    const psr::CharacterScreenMessage message = psr::BuildCharacterScreenMessage(
        registry, player, g_no_affixes, g_no_growth_curve, g_no_photon_arts, g_no_status_effects);

    REQUIRE(message.inventory[0].requirement_text.empty());
    REQUIRE(message.inventory[0].requirement_met);
}
