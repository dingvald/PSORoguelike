#include "Items/EquipmentDropRoller.h"

#include "Combat/Element.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/RaceIds.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

namespace {

using namespace psr;

entt::entity MakeArmor(Registry& registry, StatsComponent stats, int mod_slot_count = 0)
{
    entt::entity item = registry.CreateEntity();
    registry.Emplace<StatsComponent>(item, stats);
    registry.Emplace<ArmorComponent>(item, ArmorComponent{ArmorSlot::Torso, mod_slot_count});
    return item;
}

entt::entity MakeWeapon(Registry& registry, WeaponComponent weapon = WeaponComponent{})
{
    entt::entity item = registry.CreateEntity();
    registry.Emplace<WeaponComponent>(item, weapon);
    return item;
}

bool IsCanonicalRaceId(std::uint32_t race_id)
{
    for (const auto& [id_string, display_name] : kCanonicalRaces)
        if (entt::hashed_string::value(id_string.data()) == race_id)
            return true;
    return false;
}

} // namespace

TEST_CASE("RollEquipmentVariation rolls each nonzero armor stat within +/-30% of its authored value",
          "[EquipmentDropRoller]")
{
    std::mt19937 rng{1};
    for (int i = 0; i < 200; ++i)
    {
        Registry registry;
        entt::entity item = MakeArmor(registry, StatsComponent{0, 0, 0, 10, 0, 0});
        RollEquipmentVariation(registry, item, rng);

        const StatsComponent& stats = registry.GetComponent<StatsComponent>(item);
        CHECK(stats.dfp >= 7);
        CHECK(stats.dfp <= 13);
    }
}

TEST_CASE("RollEquipmentVariation leaves a zero-authored armor stat at zero", "[EquipmentDropRoller]")
{
    std::mt19937 rng{1};
    for (int i = 0; i < 20; ++i)
    {
        Registry registry;
        entt::entity item = MakeArmor(registry, StatsComponent{});
        RollEquipmentVariation(registry, item, rng);

        const StatsComponent& stats = registry.GetComponent<StatsComponent>(item);
        CHECK(stats.atp == 0);
        CHECK(stats.ata == 0);
        CHECK(stats.mst == 0);
        CHECK(stats.dfp == 0);
        CHECK(stats.evp == 0);
        CHECK(stats.lck == 0);
    }
}

TEST_CASE("RollEquipmentVariation's mod_slot_count is always 0-4 and overrides the authored value",
          "[EquipmentDropRoller]")
{
    std::mt19937 rng{1};
    bool saw_override = false;
    for (int i = 0; i < 200; ++i)
    {
        Registry registry;
        // Authored with a fixed value that should never survive the roll.
        entt::entity item = MakeArmor(registry, StatsComponent{}, /*mod_slot_count=*/1);
        RollEquipmentVariation(registry, item, rng);

        const int slots = registry.GetComponent<ArmorComponent>(item).mod_slot_count;
        CHECK(slots >= 0);
        CHECK(slots <= 4);
        if (slots != 1)
            saw_override = true;
    }
    CHECK(saw_override); // the roll actually varies, not a no-op pass-through
}

TEST_CASE("RollEquipmentVariation's grind_level is always within [0, max_grind_level]", "[EquipmentDropRoller]")
{
    std::mt19937 rng{1};
    for (int i = 0; i < 200; ++i)
    {
        Registry registry;
        WeaponComponent weapon;
        weapon.max_grind_level = 3;
        entt::entity item = MakeWeapon(registry, weapon);
        RollEquipmentVariation(registry, item, rng);

        const int grind = registry.GetComponent<WeaponComponent>(item).grind_level;
        CHECK(grind >= 0);
        CHECK(grind <= 3);
    }
}

TEST_CASE("RollEquipmentVariation never rolls a grind_level above a zero max_grind_level", "[EquipmentDropRoller]")
{
    std::mt19937 rng{1};
    for (int i = 0; i < 20; ++i)
    {
        Registry registry;
        WeaponComponent weapon;
        weapon.max_grind_level = 0;
        entt::entity item = MakeWeapon(registry, weapon);
        RollEquipmentVariation(registry, item, rng);

        CHECK(registry.GetComponent<WeaponComponent>(item).grind_level == 0);
    }
}

TEST_CASE("RollEquipmentVariation only rolls an element when none is authored", "[EquipmentDropRoller]")
{
    std::mt19937 rng{1};

    // Authored non-None element is never overwritten.
    for (int i = 0; i < 30; ++i)
    {
        Registry registry;
        WeaponComponent weapon;
        weapon.element = Element::Ice;
        entt::entity item = MakeWeapon(registry, weapon);
        RollEquipmentVariation(registry, item, rng);
        CHECK(registry.GetComponent<WeaponComponent>(item).element == Element::Ice);
    }

    // An authored-None weapon sometimes (not always, not never) picks one up.
    bool saw_none = false;
    bool saw_element = false;
    for (int i = 0; i < 100; ++i)
    {
        Registry registry;
        entt::entity item = MakeWeapon(registry);
        RollEquipmentVariation(registry, item, rng);
        if (registry.GetComponent<WeaponComponent>(item).element == Element::None)
            saw_none = true;
        else
            saw_element = true;
    }
    CHECK(saw_none);
    CHECK(saw_element);
}

TEST_CASE("RollEquipmentVariation only rolls a race bonus when none is authored", "[EquipmentDropRoller]")
{
    std::mt19937 rng{1};

    // Authored race_bonuses are never touched.
    for (int i = 0; i < 30; ++i)
    {
        Registry registry;
        WeaponComponent weapon;
        weapon.race_bonuses = {RaceBonusEntry{entt::hashed_string::value("native"), 99}};
        entt::entity item = MakeWeapon(registry, weapon);
        RollEquipmentVariation(registry, item, rng);

        const auto& bonuses = registry.GetComponent<WeaponComponent>(item).race_bonuses;
        REQUIRE(bonuses.size() == 1);
        CHECK(bonuses[0].race_id == entt::hashed_string::value("native"));
        CHECK(bonuses[0].bonus_percent == 99);
    }

    // An authored-empty weapon sometimes gets exactly one, from the
    // canonical pool, in the authored percent range.
    bool saw_empty = false;
    bool saw_bonus = false;
    for (int i = 0; i < 100; ++i)
    {
        Registry registry;
        entt::entity item = MakeWeapon(registry);
        RollEquipmentVariation(registry, item, rng);

        const auto& bonuses = registry.GetComponent<WeaponComponent>(item).race_bonuses;
        if (bonuses.empty())
        {
            saw_empty = true;
            continue;
        }
        saw_bonus = true;
        REQUIRE(bonuses.size() == 1);
        CHECK(IsCanonicalRaceId(bonuses[0].race_id));
        CHECK(bonuses[0].bonus_percent >= 5);
        CHECK(bonuses[0].bonus_percent <= 25);
    }
    CHECK(saw_empty);
    CHECK(saw_bonus);
}

TEST_CASE("RollEquipmentVariation no-ops for an item with neither ArmorComponent nor WeaponComponent",
          "[EquipmentDropRoller]")
{
    Registry registry;
    entt::entity item = registry.CreateEntity();
    std::mt19937 rng{1};
    RollEquipmentVariation(registry, item, rng); // must not crash despite no components present
    CHECK_FALSE(registry.HasComponent<ArmorComponent>(item));
    CHECK_FALSE(registry.HasComponent<WeaponComponent>(item));
}

TEST_CASE("RollEquipmentVariation is reproducible for identically-seeded RNGs", "[EquipmentDropRoller]")
{
    std::mt19937 rng_a{123};
    std::mt19937 rng_b{123};

    for (int i = 0; i < 20; ++i)
    {
        Registry registry_a;
        Registry registry_b;
        entt::entity item_a = MakeArmor(registry_a, StatsComponent{0, 0, 0, 10, 0, 0});
        entt::entity item_b = MakeArmor(registry_b, StatsComponent{0, 0, 0, 10, 0, 0});

        RollEquipmentVariation(registry_a, item_a, rng_a);
        RollEquipmentVariation(registry_b, item_b, rng_b);

        CHECK(registry_a.GetComponent<StatsComponent>(item_a).dfp ==
              registry_b.GetComponent<StatsComponent>(item_b).dfp);
        CHECK(registry_a.GetComponent<ArmorComponent>(item_a).mod_slot_count ==
              registry_b.GetComponent<ArmorComponent>(item_b).mod_slot_count);
    }
}
