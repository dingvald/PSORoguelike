#include "Items/WeaponBuilder.h"

#include "Components/WeaponComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Affix.h"
#include "Items/AffixLibrary.h"

#include <entt/core/hashed_string.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace psr {

namespace {
    constexpr int kPrefixChancePercent = 20;
    constexpr int kGrindChancePercent = 15;
    constexpr int kRaceBonusChancePercent = 10;

    constexpr int kMinGrindLevel = 1;
    constexpr int kMaxGrindLevel = 3;

    constexpr int kMinRaceBonusPercent = 5;
    constexpr int kMaxRaceBonusPercent = 15;

    // The four monster races (docs/GDD.md), same id strings RaceComponent
    // authors on enemy prefabs (e.g. booma.json's "race_id": "native").
    const std::array<std::uint32_t, 4> kRaceIds{
        entt::hashed_string::value("native"),
        entt::hashed_string::value("a_beast"),
        entt::hashed_string::value("machine"),
        entt::hashed_string::value("dark"),
    };

    bool RollPercent(int chance_percent, std::mt19937& rng)
    {
        return std::uniform_int_distribution<int>(1, 100)(rng) <= chance_percent;
    }
} // namespace

void RunWeaponBuilder(Registry& registry, entt::entity item, const AffixLibrary& affixes, std::mt19937& rng)
{
    WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(item);
    if (!weapon)
        return;

    if (RollPercent(kPrefixChancePercent, rng))
    {
        std::vector<const Affix*> prefixes;
        for (const Affix& affix : affixes.All())
            if (affix.kind == AffixKind::Prefix)
                prefixes.push_back(&affix);

        if (!prefixes.empty())
        {
            const std::size_t index = std::uniform_int_distribution<std::size_t>(0, prefixes.size() - 1)(rng);
            weapon->prefix_affix_id = prefixes[index]->id;
        }
    }

    if (RollPercent(kGrindChancePercent, rng))
        weapon->grind_level = std::uniform_int_distribution<int>(kMinGrindLevel, kMaxGrindLevel)(rng);

    if (RollPercent(kRaceBonusChancePercent, rng))
    {
        const std::size_t race_index = std::uniform_int_distribution<std::size_t>(0, kRaceIds.size() - 1)(rng);
        const int bonus_percent = std::uniform_int_distribution<int>(kMinRaceBonusPercent, kMaxRaceBonusPercent)(rng);
        weapon->race_bonuses.push_back(RaceBonusEntry{kRaceIds[race_index], bonus_percent});
    }
}

} // namespace psr
