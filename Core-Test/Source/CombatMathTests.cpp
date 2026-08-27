#include "Engine/Combat/CombatMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

TEST_CASE("ComputeHitChance is a ratio of ATA to ATA+EVP, clamped", "[CombatMath]")
{
    CHECK(psr::ComputeHitChance(50, 50) == Catch::Approx(0.5f));
    CHECK(psr::ComputeHitChance(100, 0) == Catch::Approx(0.95f)); // clamped at the max
    CHECK(psr::ComputeHitChance(0, 100) == Catch::Approx(0.05f)); // clamped at the min
    CHECK(psr::ComputeHitChance(0, 0) == Catch::Approx(0.95f));   // no data either side -- treat as a clean hit

    // Monotonic in ATA for a fixed EVP.
    CHECK(psr::ComputeHitChance(80, 40) > psr::ComputeHitChance(40, 40));
}

TEST_CASE("ComputeDamage subtracts half DFP with variance, floored at 1", "[CombatMath]")
{
    CHECK(psr::ComputeDamage(50, 20, 1.0f) == 40); // 50 - 20/2 = 40
    CHECK(psr::ComputeDamage(10, 100, 1.0f) == 1); // heavily mitigated -- never zero
    CHECK(psr::ComputeDamage(50, 20, 1.1f) == static_cast<int>(std::lround(40.0f * 1.1f)));
    CHECK(psr::ComputeDamage(50, 20, 0.9f) == static_cast<int>(std::lround(40.0f * 0.9f)));
}

TEST_CASE("ApplyRaceBonus multiplies for a matching entry only", "[CombatMath]")
{
    const std::vector<psr::RaceBonusEntry> bonuses = {{/*race_id=*/1, /*bonus_percent=*/50},
                                                        {/*race_id=*/2, /*bonus_percent=*/25}};

    CHECK(psr::ApplyRaceBonus(100, bonuses, 1) == 150);
    CHECK(psr::ApplyRaceBonus(100, bonuses, 2) == 125);
    CHECK(psr::ApplyRaceBonus(100, bonuses, 3) == 100);           // no matching entry
    CHECK(psr::ApplyRaceBonus(100, {}, 1) == 100);                // no bonuses configured at all
}
