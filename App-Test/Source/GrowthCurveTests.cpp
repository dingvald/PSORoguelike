#include "Progression/GrowthCurve.h"

#include <catch2/catch_test_macros.hpp>

#include <random>

TEST_CASE("GrowthCurve::Evaluate computes each stat from base/rate/exponent", "[GrowthCurve]")
{
    psr::GrowthCurve curve;
    curve.max_hp = {.base = 48.0f, .rate = 8.0f};
    curve.atp = {.base = 48.0f, .rate = 3.0f};

    const psr::GrowthCurveLevel level_2 = curve.Evaluate(2);
    CHECK(level_2.max_hp == 48);
    CHECK(level_2.stats.atp == 48);

    const psr::GrowthCurveLevel level_5 = curve.Evaluate(5);
    CHECK(level_5.max_hp == 48 + 8 * 3);
    CHECK(level_5.stats.atp == 48 + 3 * 3);
}

TEST_CASE("GrowthCurve::EvaluateRandomGain stays within +/-25% of the authored rate", "[GrowthCurve]")
{
    psr::GrowthCurve curve;
    curve.max_hp = {.rate = 20.0f};

    std::mt19937 rng{1};
    for (int level = 2; level < 22; ++level)
    {
        const int gain = curve.EvaluateRandomGain(level, rng).max_hp;
        CHECK(gain >= 15); // 20 * 0.75
        CHECK(gain <= 25); // 20 * 1.25
    }
}

TEST_CASE("GrowthCurve::EvaluateRandomGain actually varies across repeated rolls", "[GrowthCurve]")
{
    psr::GrowthCurve curve;
    curve.max_hp = {.rate = 20.0f};

    std::mt19937 rng{1};
    const int first = curve.EvaluateRandomGain(2, rng).max_hp;

    bool saw_a_different_value = false;
    for (int i = 0; i < 30; ++i)
    {
        if (curve.EvaluateRandomGain(2, rng).max_hp != first)
        {
            saw_a_different_value = true;
            break;
        }
    }
    CHECK(saw_a_different_value);
}

TEST_CASE("GrowthCurve::EvaluateRandomGain averages close to the authored rate over many rolls", "[GrowthCurve]")
{
    psr::GrowthCurve curve;
    curve.max_hp = {.rate = 20.0f};

    std::mt19937 rng{1};
    long long total = 0;
    constexpr int kTrials = 2000;
    for (int i = 0; i < kTrials; ++i)
        total += curve.EvaluateRandomGain(2, rng).max_hp;

    const double average = static_cast<double>(total) / kTrials;
    CHECK(average > 19.0); // within 5% of the authored rate of 20
    CHECK(average < 21.0);
}

TEST_CASE("GrowthCurve::EvaluateRandomGain's average matches the curve's level-over-level delta for a non-linear "
         "stat",
         "[GrowthCurve]")
{
    // Every stat curve authored in App/Assets/Data/Classes today is linear
    // (exponent == 1), where EvaluateRandomGain's deterministic center is
    // exactly `rate` at every level (see GrowthCurve.h). This exercises the
    // general Evaluate(level)-Evaluate(level-1) path (level >= 3) for a
    // non-linear stat curve to confirm it isn't just always returning `rate`.
    psr::GrowthCurve curve;
    curve.atp = {.base = 50.0f, .rate = 30.0f, .exponent = 1.26f};

    const int expected_gain_at_level_4 = curve.Evaluate(4).stats.atp - curve.Evaluate(3).stats.atp;
    REQUIRE(expected_gain_at_level_4 != static_cast<int>(std::lround(curve.atp.rate)));

    std::mt19937 rng{3};
    long long total = 0;
    constexpr int kTrials = 3000;
    for (int i = 0; i < kTrials; ++i)
        total += curve.EvaluateRandomGain(4, rng).stats.atp;

    const double average = static_cast<double>(total) / kTrials;
    CHECK(average > static_cast<double>(expected_gain_at_level_4) - 1.0);
    CHECK(average < static_cast<double>(expected_gain_at_level_4) + 1.0);
}

TEST_CASE("GrowthCurve::EvaluateRandomGain leaves xp_to_next deterministic", "[GrowthCurve]")
{
    psr::GrowthCurve curve;
    curve.xp_to_next = {.base = 50.0f, .rate = 30.0f, .exponent = 1.26f};
    curve.max_hp = {.rate = 20.0f};

    std::mt19937 rng{1};
    for (int level = 2; level < 6; ++level)
        CHECK(curve.EvaluateRandomGain(level, rng).xp_to_next == curve.Evaluate(level).xp_to_next);
}

TEST_CASE("GrowthCurve::EvaluateRandomGain is reproducible for identically-seeded RNGs", "[GrowthCurve]")
{
    psr::GrowthCurve curve;
    curve.max_hp = {.rate = 20.0f};
    curve.atp = {.rate = 3.0f};

    std::mt19937 rng_a{99};
    std::mt19937 rng_b{99};

    for (int level = 2; level < 6; ++level)
    {
        const psr::GrowthCurveLevel a = curve.EvaluateRandomGain(level, rng_a);
        const psr::GrowthCurveLevel b = curve.EvaluateRandomGain(level, rng_b);
        CHECK(a.max_hp == b.max_hp);
        CHECK(a.stats.atp == b.stats.atp);
    }
}
