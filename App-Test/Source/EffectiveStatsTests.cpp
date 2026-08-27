#include "Combat/EffectiveStats.h"

#include "Components/EquipmentComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/RareVariantComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>

namespace {

psr::Entity MakeActorWithStats(psr::Registry& registry, int atp, int ata)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    psr::StatsComponent stats;
    stats.atp = atp;
    stats.ata = ata;
    actor.Emplace<psr::StatsComponent>(stats);
    return actor;
}

} // namespace

TEST_CASE("ComputeEffectiveStats returns the base StatsComponent unmodified with no equipment/rare variant",
         "[EffectiveStats]")
{
    psr::Registry registry;
    psr::AffixLibrary affixes;

    psr::Entity actor = MakeActorWithStats(registry, /*atp=*/50, /*ata=*/30);

    const psr::StatsComponent total = psr::ComputeEffectiveStats(actor, affixes);
    CHECK(total.atp == 50);
    CHECK(total.ata == 30);
}

TEST_CASE("ComputeEffectiveStats ignores stat_multiplier when RareVariantComponent::is_rare is false",
         "[EffectiveStats]")
{
    psr::Registry registry;
    psr::AffixLibrary affixes;

    psr::Entity actor = MakeActorWithStats(registry, /*atp=*/50, /*ata=*/30);
    actor.Emplace<psr::RareVariantComponent>(psr::RareVariantComponent{/*is_rare=*/false, /*stat_multiplier=*/3.0f});

    const psr::StatsComponent total = psr::ComputeEffectiveStats(actor, affixes);
    CHECK(total.atp == 50);
    CHECK(total.ata == 30);
}

TEST_CASE("ComputeEffectiveStats scales every stat by stat_multiplier when is_rare is true", "[EffectiveStats]")
{
    psr::Registry registry;
    psr::AffixLibrary affixes;

    psr::Entity actor = MakeActorWithStats(registry, /*atp=*/50, /*ata=*/30);
    actor.Emplace<psr::RareVariantComponent>(psr::RareVariantComponent{/*is_rare=*/true, /*stat_multiplier=*/2.0f});

    const psr::StatsComponent total = psr::ComputeEffectiveStats(actor, affixes);
    CHECK(total.atp == 100);
    CHECK(total.ata == 60);
}

TEST_CASE("ComputeEffectiveStats applies stat_multiplier to the equipped-gear total, not just base stats",
         "[EffectiveStats]")
{
    psr::Registry registry;
    psr::AffixLibrary affixes;

    psr::Entity actor = MakeActorWithStats(registry, /*atp=*/50, /*ata=*/0);
    actor.Emplace<psr::RareVariantComponent>(psr::RareVariantComponent{/*is_rare=*/true, /*stat_multiplier=*/2.0f});

    entt::entity weapon = registry.CreateEntity();
    psr::StatsComponent weapon_bonus;
    weapon_bonus.atp = 20;
    registry.Emplace<psr::StatsComponent>(weapon, weapon_bonus);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    const psr::StatsComponent total = psr::ComputeEffectiveStats(actor, affixes);
    // (50 base + 20 equipped) * 2.0 = 140, not 50*2 + 20.
    CHECK(total.atp == 140);
}
