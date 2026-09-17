#include "Items/EquipPreview.h"

#include "Components/EquipmentComponent.h"
#include "Components/MagComponent.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>

namespace {
psr::AffixLibrary g_no_affixes;
} // namespace

TEST_CASE("ComputeEquipStatDelta reports the stat swing from equipping an unequipped weapon", "[EquipPreview]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::StatsComponent>(player, psr::StatsComponent{/*atp=*/10, /*ata=*/0, /*mst=*/0, /*dfp=*/0,
                                                                       /*evp=*/0, /*lck=*/0});
    registry.Emplace<psr::EquipmentComponent>(player);

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    registry.Emplace<psr::StatsComponent>(weapon, psr::StatsComponent{/*atp=*/5, /*ata=*/2, /*mst=*/0, /*dfp=*/0,
                                                                       /*evp=*/0, /*lck=*/0});

    const std::optional<psr::StatsComponent> delta =
        psr::ComputeEquipStatDelta(registry, player, weapon, g_no_affixes);

    REQUIRE(delta.has_value());
    CHECK(delta->atp == 5);
    CHECK(delta->ata == 2);
    CHECK(delta->dfp == 0);

    // The swap-then-restore trick must leave the real equipment untouched.
    // Double-parenthesized: entt::null_t's templated operator== is ambiguous
    // with Catch2's expression-decomposing CHECK otherwise (same workaround
    // EquipTests.cpp's own REQUIRE((... == entt::null)) already uses).
    CHECK((registry.GetComponent<psr::EquipmentComponent>(player).weapon == entt::null));
}

TEST_CASE("ComputeEquipStatDelta reports the stat swing from swapping out an already-equipped item", "[EquipPreview]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::StatsComponent>(player);

    entt::entity old_armor = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(old_armor, psr::ArmorComponent{psr::ArmorSlot::Torso, 0});
    registry.Emplace<psr::StatsComponent>(old_armor, psr::StatsComponent{/*atp=*/0, /*ata=*/0, /*mst=*/0, /*dfp=*/4,
                                                                          /*evp=*/0, /*lck=*/0});

    entt::entity new_armor = registry.CreateEntity();
    registry.Emplace<psr::ArmorComponent>(new_armor, psr::ArmorComponent{psr::ArmorSlot::Torso, 0});
    registry.Emplace<psr::StatsComponent>(new_armor, psr::StatsComponent{/*atp=*/0, /*ata=*/0, /*mst=*/0, /*dfp=*/9,
                                                                          /*evp=*/0, /*lck=*/0});

    psr::EquipmentComponent equipment;
    equipment.torso = old_armor;
    registry.Emplace<psr::EquipmentComponent>(player, equipment);

    const std::optional<psr::StatsComponent> delta =
        psr::ComputeEquipStatDelta(registry, player, new_armor, g_no_affixes);

    REQUIRE(delta.has_value());
    CHECK(delta->dfp == 5); // 9 (new) - 4 (old)

    CHECK(registry.GetComponent<psr::EquipmentComponent>(player).torso == old_armor);
}

TEST_CASE("ComputeEquipStatDelta returns nullopt for a non-equippable item", "[EquipPreview]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::EquipmentComponent>(player);

    entt::entity trinket = registry.CreateEntity(); // neither WeaponComponent nor ArmorComponent

    CHECK_FALSE(psr::ComputeEquipStatDelta(registry, player, trinket, g_no_affixes).has_value());
}

TEST_CASE("ComputeEquipStatDelta returns nullopt when the actor has no EquipmentComponent", "[EquipPreview]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);

    CHECK_FALSE(psr::ComputeEquipStatDelta(registry, player, weapon, g_no_affixes).has_value());
}

TEST_CASE("ComputeUnequipStatDelta reports the stat swing from removing the equipped mag", "[EquipPreview][Mag]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::StatsComponent>(player);

    entt::entity mag = registry.CreateEntity();
    psr::MagComponent mag_component;
    mag_component.pow_level = 4; // ATP += 4 * 2 = 8, per EffectiveStats.cpp's kMagPowToAtp
    registry.Emplace<psr::MagComponent>(mag, mag_component);

    psr::EquipmentComponent equipment;
    equipment.mag = mag;
    registry.Emplace<psr::EquipmentComponent>(player, equipment);

    const std::optional<psr::StatsComponent> delta =
        psr::ComputeUnequipStatDelta(registry, player, psr::EquipmentSlot::Mag, g_no_affixes);

    REQUIRE(delta.has_value());
    CHECK(delta->atp == -8);
    CHECK(registry.GetComponent<psr::EquipmentComponent>(player).mag == mag); // untouched afterward
}

TEST_CASE("ComputeUnequipStatDelta returns nullopt when the actor has no EquipmentComponent", "[EquipPreview]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    CHECK_FALSE(psr::ComputeUnequipStatDelta(registry, player, psr::EquipmentSlot::Mag, g_no_affixes).has_value());
}
