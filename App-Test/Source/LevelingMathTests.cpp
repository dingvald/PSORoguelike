#include "Combat/LevelingMath.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ExpRequiredForLevel grows with level per exp_base/exp_growth_exponent", "[LevelingMath]")
{
    psr::LevelingConfig config;
    config.exp_base = 100;
    config.exp_growth_exponent = 1.5f;

    const int level_1 = psr::ExpRequiredForLevel(1, config);
    const int level_2 = psr::ExpRequiredForLevel(2, config);
    const int level_10 = psr::ExpRequiredForLevel(10, config);

    REQUIRE(level_1 == 100);
    REQUIRE(level_2 > level_1);
    REQUIRE(level_10 > level_2);
}

TEST_CASE("ExpRequiredForLevel scales with exp_base", "[LevelingMath]")
{
    psr::LevelingConfig small;
    small.exp_base = 50;
    small.exp_growth_exponent = 1.5f;

    psr::LevelingConfig large;
    large.exp_base = 200;
    large.exp_growth_exponent = 1.5f;

    REQUIRE(psr::ExpRequiredForLevel(5, large) > psr::ExpRequiredForLevel(5, small));
}
