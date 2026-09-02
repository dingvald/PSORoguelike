#include "Items/CharacterScreenSnapshot.h"

#include "Combat/LevelingConfig.h"
#include "Combat/LevelingMath.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/LevelComponent.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace psr;

entt::entity MakeWeapon(Registry& registry, int atp)
{
    entt::entity weapon = registry.CreateEntity();
    StatsComponent stats;
    stats.atp = atp;
    registry.Emplace<StatsComponent>(weapon, stats);
    registry.Emplace<WeaponComponent>(weapon);
    return weapon;
}

} // namespace

TEST_CASE("BuildCharacterScreenMessage reports level/exp fields from the player's LevelComponent", "[CharacterScreenSnapshot]")
{
    Registry registry;
    AffixLibrary affixes;
    LevelingConfig leveling;
    leveling.exp_base = 100;
    leveling.exp_growth_exponent = 1.0f;

    entt::entity player = registry.CreateEntity();
    registry.Emplace<LevelComponent>(player, LevelComponent{5, 250});

    const CharacterScreenMessage message = BuildCharacterScreenMessage(registry, player, affixes, leveling);

    CHECK(message.level == 5);
    CHECK(message.current_exp == 250);
    CHECK(message.exp_to_next_level == ExpRequiredForLevel(6, leveling));
}

TEST_CASE("BuildCharacterScreenMessage defaults level to 1 when the player has no LevelComponent yet",
          "[CharacterScreenSnapshot]")
{
    Registry registry;
    AffixLibrary affixes;
    LevelingConfig leveling;

    entt::entity player = registry.CreateEntity();

    const CharacterScreenMessage message = BuildCharacterScreenMessage(registry, player, affixes, leveling);

    CHECK(message.level == 1);
    CHECK(message.current_exp == 0);
}

TEST_CASE("BuildCharacterScreenMessage fills preview_value only for a focused inventory item that resolves to a slot",
          "[CharacterScreenSnapshot]")
{
    Registry registry;
    AffixLibrary affixes;
    LevelingConfig leveling;

    entt::entity player = registry.CreateEntity();
    registry.Emplace<StatsComponent>(player, StatsComponent{});
    const entt::entity equipped_weapon = MakeWeapon(registry, /*atp=*/10);
    const entt::entity better_weapon = MakeWeapon(registry, /*atp=*/40);
    registry.Emplace<EquipmentComponent>(player, EquipmentComponent{equipped_weapon});
    registry.Emplace<InventoryComponent>(player, InventoryComponent{{better_weapon}, InventoryComponent::kDefaultCapacity});

    const CharacterScreenMessage message =
        BuildCharacterScreenMessage(registry, player, affixes, leveling, CharacterScreenFocus{0, std::nullopt});

    REQUIRE(message.stats[0].label == "ATP");
    CHECK(message.stats[0].current_value == 10);
    REQUIRE(message.stats[0].preview_value.has_value());
    CHECK(*message.stats[0].preview_value == 40);
    CHECK(message.focus.inventory_index == 0);
}

TEST_CASE("BuildCharacterScreenMessage previews nothing for a non-equippable inventory item", "[CharacterScreenSnapshot]")
{
    Registry registry;
    AffixLibrary affixes;
    LevelingConfig leveling;

    entt::entity player = registry.CreateEntity();
    registry.Emplace<StatsComponent>(player, StatsComponent{});
    const entt::entity not_equippable = registry.CreateEntity(); // no WeaponComponent/ArmorComponent
    registry.Emplace<InventoryComponent>(player,
                                         InventoryComponent{{not_equippable}, InventoryComponent::kDefaultCapacity});

    const CharacterScreenMessage message =
        BuildCharacterScreenMessage(registry, player, affixes, leveling, CharacterScreenFocus{0, std::nullopt});

    for (const CharacterScreenMessage::StatEntry& stat : message.stats)
        CHECK_FALSE(stat.preview_value.has_value());
}

TEST_CASE("BuildCharacterScreenMessage previews nothing when no focus is given", "[CharacterScreenSnapshot]")
{
    Registry registry;
    AffixLibrary affixes;
    LevelingConfig leveling;

    entt::entity player = registry.CreateEntity();
    registry.Emplace<StatsComponent>(player, StatsComponent{});
    const entt::entity weapon = MakeWeapon(registry, /*atp=*/40);
    registry.Emplace<InventoryComponent>(player, InventoryComponent{{weapon}, InventoryComponent::kDefaultCapacity});

    const CharacterScreenMessage message = BuildCharacterScreenMessage(registry, player, affixes, leveling);

    for (const CharacterScreenMessage::StatEntry& stat : message.stats)
        CHECK_FALSE(stat.preview_value.has_value());
    CHECK_FALSE(message.focus.inventory_index.has_value());
    CHECK_FALSE(message.focus.equipment_slot.has_value());
}

TEST_CASE("BuildCharacterScreenMessage echoes back an equipment-slot focus with no preview",
          "[CharacterScreenSnapshot]")
{
    Registry registry;
    AffixLibrary affixes;
    LevelingConfig leveling;

    entt::entity player = registry.CreateEntity();
    registry.Emplace<StatsComponent>(player, StatsComponent{});
    const entt::entity weapon = MakeWeapon(registry, /*atp=*/10);
    registry.Emplace<EquipmentComponent>(player, EquipmentComponent{weapon});

    const CharacterScreenMessage message = BuildCharacterScreenMessage(
        registry, player, affixes, leveling, CharacterScreenFocus{std::nullopt, EquipmentSlot::Weapon});

    REQUIRE(message.focus.equipment_slot.has_value());
    CHECK(*message.focus.equipment_slot == EquipmentSlot::Weapon);
    for (const CharacterScreenMessage::StatEntry& stat : message.stats)
        CHECK_FALSE(stat.preview_value.has_value());
}
