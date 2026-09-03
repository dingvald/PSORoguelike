#include "Engine/Math/Easing.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("EaseOutQuad maps 0 to 0 and 1 to 1", "[Easing]")
{
    REQUIRE(psr::EaseOutQuad(0.0f) == 0.0f);
    REQUIRE(psr::EaseOutQuad(1.0f) == 1.0f);
}

TEST_CASE("EaseOutQuad is monotonically increasing", "[Easing]")
{
    REQUIRE(psr::EaseOutQuad(0.25f) < psr::EaseOutQuad(0.5f));
    REQUIRE(psr::EaseOutQuad(0.5f) < psr::EaseOutQuad(0.75f));
}

TEST_CASE("EaseOutQuad starts faster than linear (front-loaded)", "[Easing]")
{
    REQUIRE(psr::EaseOutQuad(0.25f) > 0.25f);
}

TEST_CASE("EaseInQuad maps 0 to 0 and 1 to 1", "[Easing]")
{
    REQUIRE(psr::EaseInQuad(0.0f) == 0.0f);
    REQUIRE(psr::EaseInQuad(1.0f) == 1.0f);
}

TEST_CASE("EaseInQuad is monotonically increasing", "[Easing]")
{
    REQUIRE(psr::EaseInQuad(0.25f) < psr::EaseInQuad(0.5f));
    REQUIRE(psr::EaseInQuad(0.5f) < psr::EaseInQuad(0.75f));
}

TEST_CASE("EaseInQuad starts slower than linear (back-loaded)", "[Easing]")
{
    REQUIRE(psr::EaseInQuad(0.25f) < 0.25f);
}

TEST_CASE("Linear maps t to itself", "[Easing]")
{
    REQUIRE(psr::Linear(0.0f) == 0.0f);
    REQUIRE(psr::Linear(0.25f) == 0.25f);
    REQUIRE(psr::Linear(1.0f) == 1.0f);
}

TEST_CASE("Ease dispatches to the matching curve", "[Easing]")
{
    REQUIRE(psr::Ease(psr::EasingCurve::Linear, 0.25f) == psr::Linear(0.25f));
    REQUIRE(psr::Ease(psr::EasingCurve::EaseOutQuad, 0.25f) == psr::EaseOutQuad(0.25f));
    REQUIRE(psr::Ease(psr::EasingCurve::EaseInQuad, 0.25f) == psr::EaseInQuad(0.25f));
}
