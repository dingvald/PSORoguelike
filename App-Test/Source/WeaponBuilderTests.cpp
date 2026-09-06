#include "Items/WeaponBuilder.h"

#include "Components/WeaponComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Affix.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace {

psr::AffixLibrary MakePrefixLibrary()
{
    std::vector<psr::Affix> affixes;

    psr::Affix draw;
    draw.id = 1;
    draw.id_string = "draw";
    draw.name = "Draw";
    draw.kind = psr::AffixKind::Prefix;
    affixes.push_back(draw);

    psr::Affix heart;
    heart.id = 2;
    heart.id_string = "heart";
    heart.name = "Heart";
    heart.kind = psr::AffixKind::Prefix;
    affixes.push_back(heart);

    psr::Affix suffix;
    suffix.id = 3;
    suffix.id_string = "power";
    suffix.name = "Power";
    suffix.kind = psr::AffixKind::Suffix; // present to prove the roll never picks a suffix
    affixes.push_back(suffix);

    return psr::AffixLibrary{std::move(affixes)};
}

bool IsKnownRaceId(std::uint32_t race_id)
{
    static const std::array<std::uint32_t, 4> kKnownRaceIds{
        entt::hashed_string::value("native"),
        entt::hashed_string::value("a_beast"),
        entt::hashed_string::value("machine"),
        entt::hashed_string::value("dark"),
    };
    for (const std::uint32_t known : kKnownRaceIds)
        if (known == race_id)
            return true;
    return false;
}

} // namespace

TEST_CASE("RunWeaponBuilder leaves an item with no WeaponComponent untouched", "[WeaponBuilder]")
{
    psr::Registry registry;
    const psr::AffixLibrary affixes = MakePrefixLibrary();
    std::mt19937 rng{1};

    const entt::entity item = registry.CreateEntity();
    RunWeaponBuilder(registry, item, affixes, rng);

    REQUIRE(registry.TryGetComponent<psr::WeaponComponent>(item) == nullptr);
}

TEST_CASE("RunWeaponBuilder only ever produces values within their documented ranges", "[WeaponBuilder]")
{
    const psr::AffixLibrary affixes = MakePrefixLibrary();

    bool saw_prefix = false;
    bool saw_grind = false;
    bool saw_race_bonus = false;
    bool saw_nothing = false;

    for (unsigned seed = 0; seed < 500; ++seed)
    {
        psr::Registry registry;
        std::mt19937 rng{seed};

        const entt::entity item = registry.CreateEntity();
        registry.Emplace<psr::WeaponComponent>(item, psr::WeaponComponent{});

        RunWeaponBuilder(registry, item, affixes, rng);

        const psr::WeaponComponent* weapon = registry.TryGetComponent<psr::WeaponComponent>(item);
        REQUIRE(weapon != nullptr);

        REQUIRE(weapon->grind_level >= 0);
        REQUIRE(weapon->grind_level <= 3);

        if (weapon->prefix_affix_id != 0)
        {
            const psr::Affix* prefix = affixes.Find(weapon->prefix_affix_id);
            REQUIRE(prefix != nullptr);
            REQUIRE(prefix->kind == psr::AffixKind::Prefix);
            saw_prefix = true;
        }

        if (weapon->grind_level != 0)
            saw_grind = true;

        REQUIRE(weapon->race_bonuses.size() <= 1);
        if (!weapon->race_bonuses.empty())
        {
            const psr::RaceBonusEntry& entry = weapon->race_bonuses.front();
            REQUIRE(IsKnownRaceId(entry.race_id));
            REQUIRE(entry.bonus_percent >= 5);
            REQUIRE(entry.bonus_percent <= 15);
            saw_race_bonus = true;
        }

        if (weapon->prefix_affix_id == 0 && weapon->grind_level == 0 && weapon->race_bonuses.empty())
            saw_nothing = true;
    }

    CHECK(saw_prefix);
    CHECK(saw_grind);
    CHECK(saw_race_bonus);
    CHECK(saw_nothing);
}

TEST_CASE("RunWeaponBuilder never sets a prefix when the affix library is empty", "[WeaponBuilder]")
{
    const psr::AffixLibrary empty_affixes;

    for (unsigned seed = 0; seed < 200; ++seed)
    {
        psr::Registry registry;
        std::mt19937 rng{seed};

        const entt::entity item = registry.CreateEntity();
        registry.Emplace<psr::WeaponComponent>(item, psr::WeaponComponent{});

        RunWeaponBuilder(registry, item, empty_affixes, rng);

        const psr::WeaponComponent* weapon = registry.TryGetComponent<psr::WeaponComponent>(item);
        REQUIRE(weapon != nullptr);
        REQUIRE(weapon->prefix_affix_id == 0);
    }
}
