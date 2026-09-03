#include "Combat/CombatMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

TEST_CASE("ComputeHitChance is Accuracy = ATA - EVP*0.2, clamped to [0, 100]", "[CombatMath]")
{
    CHECK(psr::ComputeHitChance(50, 50) == Catch::Approx(0.4f)); // 50 - 10 = 40 -> 0.40
    CHECK(psr::ComputeHitChance(120, 0) == Catch::Approx(1.0f)); // clamped -- a guaranteed hit is possible
    CHECK(psr::ComputeHitChance(0, 100) == Catch::Approx(0.0f)); // clamped -- a guaranteed miss is possible
    CHECK(psr::ComputeHitChance(0, 0) == Catch::Approx(0.0f));   // no ATA at all -- a guaranteed miss

    // Monotonic in ATA for a fixed EVP.
    CHECK(psr::ComputeHitChance(80, 40) > psr::ComputeHitChance(40, 40));
}

TEST_CASE("ComboAtaMultiplier compounds 1.3x per successive hit", "[CombatMath]")
{
    CHECK(psr::ComboAtaMultiplier(0) == Catch::Approx(1.0f));
    CHECK(psr::ComboAtaMultiplier(1) == Catch::Approx(1.3f));
    CHECK(psr::ComboAtaMultiplier(2) == Catch::Approx(1.69f));
}

TEST_CASE("ComputeDamage floors ((ATP - DFP) / 5) * 0.9 * variance, floored at 1", "[CombatMath]")
{
    CHECK(psr::ComputeDamage(70, 20, 1.0f) == 9);  // floor((70-20)/5 * 0.9) = floor(9.0) = 9
    CHECK(psr::ComputeDamage(10, 100, 1.0f) == 1); // heavily mitigated -- never zero
    CHECK(psr::ComputeDamage(70, 20, 1.1f) == static_cast<int>(std::floor(10.0f * 0.9f * 1.1f)));
    CHECK(psr::ComputeDamage(70, 20, 0.9f) == static_cast<int>(std::floor(10.0f * 0.9f * 0.9f)));
}

TEST_CASE("ComputeTechniqueDamage floors (MST / 5) * (1 - resistance%), no DFP term, floored at 1", "[CombatMath]")
{
    CHECK(psr::ComputeTechniqueDamage(50, 0) == 10);  // floor(50/5) = 10
    CHECK(psr::ComputeTechniqueDamage(50, 50) == 5);  // half resisted
    CHECK(psr::ComputeTechniqueDamage(50, 100) == 1); // fully resisted -- never zero
    CHECK(psr::ComputeTechniqueDamage(1, 0) == 1);    // never zero on a landed cast
}

TEST_CASE("ComputeCritChance is LCK/500 clamped to [0, 1]", "[CombatMath]")
{
    CHECK(psr::ComputeCritChance(0) == Catch::Approx(0.0f));
    CHECK(psr::ComputeCritChance(50) == Catch::Approx(0.1f));
    CHECK(psr::ComputeCritChance(1000) == Catch::Approx(1.0f)); // clamped
}

TEST_CASE("ApplyCritical multiplies by 1.5 only when critical", "[CombatMath]")
{
    CHECK(psr::ApplyCritical(100, false) == 100);
    CHECK(psr::ApplyCritical(100, true) == 150);
    CHECK(psr::ApplyCritical(11, true) == static_cast<int>(std::lround(11.0f * 1.5f)));
}

TEST_CASE("ApplyRaceBonus multiplies for a matching entry only", "[CombatMath]")
{
    const std::vector<psr::RaceBonusEntry> bonuses = {{/*race_id=*/1, /*bonus_percent=*/50},
                                                      {/*race_id=*/2, /*bonus_percent=*/25}};

    CHECK(psr::ApplyRaceBonus(100, bonuses, 1) == 150);
    CHECK(psr::ApplyRaceBonus(100, bonuses, 2) == 125);
    CHECK(psr::ApplyRaceBonus(100, bonuses, 3) == 100); // no matching entry
    CHECK(psr::ApplyRaceBonus(100, {}, 1) == 100);      // no bonuses configured at all
}
