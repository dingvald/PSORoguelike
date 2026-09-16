#include "Items/EquipmentDropRoller.h"

#include "Combat/Element.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/RaceIds.h"

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace psr {

namespace {

    // Tuning knobs -- no design doc pins these exact numbers down, they're a
    // reasonable starting point for the roll shapes the feature asks for.
    constexpr float kArmorStatVarianceFraction = 0.30f;
    constexpr double kSlotFalloffP = 0.5;    // geometric_distribution success prob: P(0)=0.5, P(1)=0.25, ...
    constexpr double kGrindFalloffP = 0.5;   // same falloff shape as the slot roll
    constexpr double kElementRollChance = 0.35;
    constexpr double kRaceBonusRollChance = 0.40;
    constexpr int kRaceBonusPercentMin = 5;
    constexpr int kRaceBonusPercentMax = 25;
    constexpr int kMaxModSlotCount = 4;

    constexpr std::array<Element, 5> kRollableElements = {Element::Fire, Element::Ice, Element::Lightning,
                                                           Element::Light, Element::Dark};

    void RollTriangular(int& value, std::mt19937& rng)
    {
        if (value == 0)
            return;

        const float mode = static_cast<float>(value);
        const float lo = mode * (1.0f - kArmorStatVarianceFraction);
        const float hi = mode * (1.0f + kArmorStatVarianceFraction);
        const std::array<float, 3> intervals = {lo, mode, hi};
        const std::array<float, 3> weights = {0.0f, 1.0f, 0.0f};
        std::piecewise_linear_distribution<float> roll(intervals.begin(), intervals.end(), weights.begin());
        value = static_cast<int>(std::lround(roll(rng)));
    }

    void RollArmorStats(StatsComponent& stats, std::mt19937& rng)
    {
        RollTriangular(stats.atp, rng);
        RollTriangular(stats.ata, rng);
        RollTriangular(stats.mst, rng);
        RollTriangular(stats.dfp, rng);
        RollTriangular(stats.evp, rng);
        RollTriangular(stats.lck, rng);
    }

    void RollWeaponElement(WeaponComponent& weapon, std::mt19937& rng)
    {
        if (weapon.element != Element::None)
            return;
        if (!std::bernoulli_distribution(kElementRollChance)(rng))
            return;
        std::uniform_int_distribution<int> pick(0, static_cast<int>(kRollableElements.size()) - 1);
        weapon.element = kRollableElements[static_cast<std::size_t>(pick(rng))];
    }

    void RollWeaponRaceBonus(WeaponComponent& weapon, std::mt19937& rng)
    {
        if (!weapon.race_bonuses.empty())
            return;
        if (!std::bernoulli_distribution(kRaceBonusRollChance)(rng))
            return;

        std::uniform_int_distribution<int> pick_race(0, static_cast<int>(kCanonicalRaces.size()) - 1);
        std::uniform_int_distribution<int> pick_percent(kRaceBonusPercentMin, kRaceBonusPercentMax);

        const std::string_view race_id_string = kCanonicalRaces[static_cast<std::size_t>(pick_race(rng))].first;

        RaceBonusEntry entry;
        entry.race_id = entt::hashed_string::value(race_id_string.data());
        entry.bonus_percent = pick_percent(rng);
        weapon.race_bonuses.push_back(entry);
    }

} // namespace

void RollEquipmentVariation(Registry& registry, entt::entity item, std::mt19937& rng)
{
    if (registry.HasComponent<ArmorComponent>(item))
    {
        if (StatsComponent* stats = registry.TryGetComponent<StatsComponent>(item))
            RollArmorStats(*stats, rng);

        ArmorComponent& armor = registry.GetComponent<ArmorComponent>(item);
        armor.mod_slot_count = std::min(std::geometric_distribution<int>(kSlotFalloffP)(rng), kMaxModSlotCount);
    }

    if (registry.HasComponent<WeaponComponent>(item))
    {
        WeaponComponent& weapon = registry.GetComponent<WeaponComponent>(item);
        weapon.grind_level =
            std::min(std::geometric_distribution<int>(kGrindFalloffP)(rng), weapon.max_grind_level);
        RollWeaponElement(weapon, rng);
        RollWeaponRaceBonus(weapon, rng);
    }
}

} // namespace psr
