#include "Combat/EffectiveStats.h"

#include "Components/EquipmentComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace psr;

entt::entity MakeWeapon(Registry& registry, int atp, std::uint32_t prefix_affix_id = 0)
{
    entt::entity weapon = registry.CreateEntity();
    StatsComponent stats;
    stats.atp = atp;
    registry.Emplace<StatsComponent>(weapon, stats);
    WeaponComponent weapon_component;
    weapon_component.prefix_affix_id = prefix_affix_id;
    registry.Emplace<WeaponComponent>(weapon, weapon_component);
    return weapon;
}

} // namespace

TEST_CASE("ComputeEffectiveStatsWithSlotOverride swaps only the targeted slot", "[EffectiveStats]")
{
    Registry registry;
    AffixLibrary affixes;

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    StatsComponent base;
    base.dfp = 5;
    actor.Emplace<StatsComponent>(base);

    const entt::entity equipped_weapon = MakeWeapon(registry, /*atp=*/10);
    const entt::entity hypothetical_weapon = MakeWeapon(registry, /*atp=*/40);
    actor.Emplace<EquipmentComponent>(EquipmentComponent{equipped_weapon});

    const StatsComponent real = ComputeEffectiveStats(actor, affixes);
    REQUIRE(real.atp == 10);
    REQUIRE(real.dfp == 5);

    const StatsComponent preview =
        ComputeEffectiveStatsWithSlotOverride(actor, affixes, EquipmentSlot::Weapon, hypothetical_weapon);
    CHECK(preview.atp == 40); // the hypothetical weapon's own bonus, not the real one's
    CHECK(preview.dfp == 5);  // base/other slots untouched

    // The real EquipmentComponent must not have been mutated by the preview.
    CHECK(ComputeEffectiveStats(actor, affixes).atp == 10);
}

TEST_CASE("ComputeEffectiveStatsWithSlotOverride applies the hypothetical weapon's own affix bonus", "[EffectiveStats]")
{
    Registry registry;
    Affix affix;
    affix.id = 1;
    affix.stat = AffixStat::Atp;
    affix.amount = 25;
    AffixLibrary affixes{std::vector<Affix>{affix}};

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<StatsComponent>(StatsComponent{});

    const entt::entity plain_weapon = MakeWeapon(registry, /*atp=*/10);
    const entt::entity affixed_weapon = MakeWeapon(registry, /*atp=*/10, /*prefix_affix_id=*/affix.id);
    actor.Emplace<EquipmentComponent>(EquipmentComponent{plain_weapon});

    const StatsComponent preview =
        ComputeEffectiveStatsWithSlotOverride(actor, affixes, EquipmentSlot::Weapon, affixed_weapon);

    CHECK(preview.atp == 10 + 25); // weapon's own atp plus its prefix affix, not the equipped weapon's (none)
    CHECK(ComputeEffectiveStats(actor, affixes).atp == 10); // real loadout still has no affix bonus
}

TEST_CASE("ComputeEffectiveStatsWithSlotOverride on an actor with no EquipmentComponent starts from an empty loadout",
          "[EffectiveStats]")
{
    Registry registry;
    AffixLibrary affixes;

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    StatsComponent base;
    base.atp = 7;
    actor.Emplace<StatsComponent>(base);

    const entt::entity weapon = MakeWeapon(registry, /*atp=*/15);

    const StatsComponent preview =
        ComputeEffectiveStatsWithSlotOverride(actor, affixes, EquipmentSlot::Weapon, weapon);

    CHECK(preview.atp == 7 + 15);
    CHECK_FALSE(actor.Has<EquipmentComponent>()); // still no real EquipmentComponent was created
}
