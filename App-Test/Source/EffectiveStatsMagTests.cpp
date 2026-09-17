#include "Combat/EffectiveStats.h"

#include "Components/EquipmentComponent.h"
#include "Components/MagComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>

namespace {
psr::AffixLibrary g_no_affixes;
} // namespace

TEST_CASE("ComputeEffectiveStats applies the equipped mag's stat conversion ratios", "[EffectiveStats][Mag]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity mag = registry.CreateEntity();
    psr::MagComponent mag_component;
    mag_component.pow_level = 5;  // ATP += 5 * 2 = 10
    mag_component.def_level = 3;  // DFP += 3 * 1 = 3
    mag_component.mind_level = 4; // MST += 4 * 2 = 8
    mag_component.dex_level = 7;  // ATA += 7 / 2 = 3 (integer division)
    registry.Emplace<psr::MagComponent>(mag, mag_component);

    psr::EquipmentComponent equipment;
    equipment.mag = mag;
    registry.Emplace<psr::EquipmentComponent>(player, equipment);

    const psr::StatsComponent stats = psr::ComputeEffectiveStats(psr::Entity(registry, player), g_no_affixes);

    CHECK(stats.atp == 10);
    CHECK(stats.dfp == 3);
    CHECK(stats.mst == 8);
    CHECK(stats.ata == 3);
}

TEST_CASE("ComputeEffectiveStats's mag ATA conversion floors an odd DEX level", "[EffectiveStats][Mag]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();

    entt::entity mag = registry.CreateEntity();
    psr::MagComponent mag_component;
    mag_component.dex_level = 1; // 1 / 2 == 0, not rounded up
    registry.Emplace<psr::MagComponent>(mag, mag_component);

    psr::EquipmentComponent equipment;
    equipment.mag = mag;
    registry.Emplace<psr::EquipmentComponent>(player, equipment);

    const psr::StatsComponent stats = psr::ComputeEffectiveStats(psr::Entity(registry, player), g_no_affixes);

    CHECK(stats.ata == 0);
}

TEST_CASE("ComputeEffectiveStats contributes nothing extra when no mag is equipped", "[EffectiveStats][Mag]")
{
    psr::Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::EquipmentComponent>(player);

    const psr::StatsComponent stats = psr::ComputeEffectiveStats(psr::Entity(registry, player), g_no_affixes);

    CHECK(stats.atp == 0);
    CHECK(stats.ata == 0);
    CHECK(stats.mst == 0);
    CHECK(stats.dfp == 0);
}
