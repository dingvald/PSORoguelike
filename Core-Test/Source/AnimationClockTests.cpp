#include "Engine/Render/AnimationClock.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr auto kEntityA = static_cast<entt::entity>(1);
constexpr auto kEntityB = static_cast<entt::entity>(2);

} // namespace

TEST_CASE("AnimationClock::GetFrameIndex returns 0 for frame_count <= 1 or frame_time <= 0", "[AnimationClock]")
{
    psr::AnimationClock clock;

    REQUIRE(clock.GetFrameIndex(0.1f, 1) == 0);
    REQUIRE(clock.GetFrameIndex(0.0f, 4) == 0);
    REQUIRE(clock.GetFrameIndex(kEntityA, 0.1f, 1) == 0);
    REQUIRE(clock.GetFrameIndex(kEntityA, 0.0f, 4) == 0);
}

TEST_CASE("AnimationClock synced mode advances two entities sharing frame_time in lock-step", "[AnimationClock]")
{
    psr::AnimationClock clock;

    REQUIRE(clock.GetFrameIndex(0.1f, 4) == 0);
    clock.Update(0.1f);

    REQUIRE(clock.GetFrameIndex(0.1f, 4) == 1);
}

TEST_CASE("AnimationClock unsynced mode keys entities independently even with the same frame_time",
          "[AnimationClock]")
{
    psr::AnimationClock clock;

    REQUIRE(clock.GetFrameIndex(kEntityA, 0.1f, 4) == 0);
    clock.Update(0.1f);
    clock.Update(0.1f);
    REQUIRE(clock.GetFrameIndex(kEntityA, 0.1f, 4) == 2);

    // Entity B is only queried now, well after A's clock has advanced -- it
    // must start from frame 0 rather than inheriting A's tick.
    REQUIRE(clock.GetFrameIndex(kEntityB, 0.1f, 4) == 0);
}

TEST_CASE("AnimationClock::Forget resets an unsynced entity's bucket", "[AnimationClock]")
{
    psr::AnimationClock clock;

    REQUIRE(clock.GetFrameIndex(kEntityA, 0.1f, 4) == 0);
    clock.Update(0.1f);
    REQUIRE(clock.GetFrameIndex(kEntityA, 0.1f, 4) == 1);

    clock.Forget(kEntityA);

    REQUIRE(clock.GetFrameIndex(kEntityA, 0.1f, 4) == 0);
}
