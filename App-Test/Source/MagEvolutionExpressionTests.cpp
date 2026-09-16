#include "Items/Mag/MagEvolutionExpression.h"

#include "Components/MagComponent.h"

#include <catch2/catch_test_macros.hpp>

namespace {
psr::MagComponent MakeMag(int pow, int def, int dex, int mind, int iq = 0)
{
    psr::MagComponent mag;
    mag.pow_level = pow;
    mag.def_level = def;
    mag.dex_level = dex;
    mag.mind_level = mind;
    mag.iq = iq;
    return mag;
}
} // namespace

TEST_CASE("EvaluateMagEvolutionCondition evaluates a simple stat comparison", "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(/*pow=*/10, /*def=*/4, /*dex=*/0, /*mind=*/0);
    CHECK(psr::EvaluateMagEvolutionCondition("POW > DEF", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("DEF > POW", mag));
}

TEST_CASE("EvaluateMagEvolutionCondition evaluates arithmetic on both sides", "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(/*pow=*/5, /*def=*/1, /*dex=*/3, /*mind=*/2);
    // POW + DEX = 8, MIND + DEF = 3
    CHECK(psr::EvaluateMagEvolutionCondition("POW + DEX > MIND + DEF", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW + DEX < MIND + DEF", mag));
}

TEST_CASE("EvaluateMagEvolutionCondition supports every comparison operator", "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(/*pow=*/5, /*def=*/5, /*dex=*/0, /*mind=*/0);
    CHECK(psr::EvaluateMagEvolutionCondition("POW == DEF", mag));
    CHECK(psr::EvaluateMagEvolutionCondition("POW >= DEF", mag));
    CHECK(psr::EvaluateMagEvolutionCondition("POW <= DEF", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW != DEF", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW > DEF", mag));
}

TEST_CASE("EvaluateMagEvolutionCondition supports multiplication, division, parentheses, and unary minus",
          "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(/*pow=*/4, /*def=*/2, /*dex=*/0, /*mind=*/0);
    CHECK(psr::EvaluateMagEvolutionCondition("POW * 2 > DEF * 3", mag));   // 8 > 6
    CHECK(psr::EvaluateMagEvolutionCondition("POW / 2 == DEF", mag));     // 2 == 2
    CHECK(psr::EvaluateMagEvolutionCondition("(POW + DEF) * 2 == 12", mag)); // (4+2)*2 == 12
    CHECK(psr::EvaluateMagEvolutionCondition("-POW < 0", mag));
}

TEST_CASE("EvaluateMagEvolutionCondition resolves LEVEL as the sum of the four stat levels", "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(/*pow=*/1, /*def=*/2, /*dex=*/3, /*mind=*/4);
    CHECK(psr::EvaluateMagEvolutionCondition("LEVEL == 10", mag));
}

TEST_CASE("EvaluateMagEvolutionCondition resolves IQ", "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(0, 0, 0, 0, /*iq=*/12);
    CHECK(psr::EvaluateMagEvolutionCondition("IQ >= 10", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("IQ >= 100", mag));
}

TEST_CASE("EvaluateMagEvolutionCondition identifiers are case-insensitive", "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(/*pow=*/10, /*def=*/0, /*dex=*/0, /*mind=*/0);
    CHECK(psr::EvaluateMagEvolutionCondition("pow > def", mag));
}

TEST_CASE("EvaluateMagEvolutionCondition returns false for malformed or invalid conditions",
          "[MagEvolutionExpression]")
{
    const psr::MagComponent mag = MakeMag(1, 1, 1, 1);
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW +", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW", mag)); // no comparison operator
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW > (DEF", mag)); // unmatched paren
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW > UNKNOWNSTAT", mag));
    CHECK_FALSE(psr::EvaluateMagEvolutionCondition("POW > 1 extra", mag)); // trailing garbage
}
