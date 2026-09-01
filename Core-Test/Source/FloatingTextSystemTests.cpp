#include "Engine/Render/FloatingTextSystem.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("FloatingTextSystem::Spawn adds an instance with the given parameters", "[FloatingTextSystem]")
{
    psr::FloatingTextSystem system;

    system.Spawn(psr::Vec2{3, 4}, "12", psr::Color{255, 255, 255}, psr::Vec2f{0.0f, -1.0f}, 2.0f, 1.5f);

    REQUIRE(system.Active().size() == 1);
    const psr::FloatingTextInstance& instance = system.Active()[0];
    REQUIRE(instance.origin_tile == psr::Vec2{3, 4});
    REQUIRE(instance.offset == psr::Vec2f{0.0f, 0.0f});
    REQUIRE(instance.direction == psr::Vec2f{0.0f, -1.0f});
    REQUIRE(instance.speed == 2.0f);
    REQUIRE(instance.text == "12");
    REQUIRE(instance.color == psr::Color{255, 255, 255});
    REQUIRE(instance.duration == 1.5f);
    REQUIRE(instance.elapsed == 0.0f);
}

TEST_CASE("FloatingTextSystem::Update advances offset by direction * speed * delta_time", "[FloatingTextSystem]")
{
    psr::FloatingTextSystem system;
    system.Spawn(psr::Vec2{0, 0}, "5", psr::Color{255, 0, 0}, psr::Vec2f{0.0f, -1.0f}, 2.0f, 10.0f);

    system.Update(0.5f);

    REQUIRE(system.Active().size() == 1);
    REQUIRE(system.Active()[0].offset == psr::Vec2f{0.0f, -1.0f});
    REQUIRE(system.Active()[0].elapsed == 0.5f);

    system.Update(0.5f);

    REQUIRE(system.Active()[0].offset == psr::Vec2f{0.0f, -2.0f});
    REQUIRE(system.Active()[0].elapsed == 1.0f);
}

TEST_CASE("FloatingTextSystem::Update removes an instance once elapsed reaches duration", "[FloatingTextSystem]")
{
    psr::FloatingTextSystem system;
    system.Spawn(psr::Vec2{0, 0}, "5", psr::Color{255, 0, 0}, psr::Vec2f{0.0f, -1.0f}, 1.0f, 1.0f);

    system.Update(0.9f);
    REQUIRE(system.Active().size() == 1);

    system.Update(0.2f); // elapsed now 1.1s, past the 1.0s duration
    REQUIRE(system.Active().empty());
}

TEST_CASE("FloatingTextSystem tracks and expires multiple instances independently", "[FloatingTextSystem]")
{
    psr::FloatingTextSystem system;
    system.Spawn(psr::Vec2{0, 0}, "short", psr::Color{255, 0, 0}, psr::Vec2f{0.0f, -1.0f}, 1.0f, 0.5f);
    system.Spawn(psr::Vec2{1, 1}, "long", psr::Color{0, 255, 0}, psr::Vec2f{0.0f, -1.0f}, 1.0f, 2.0f);

    system.Update(0.6f); // past the short instance's duration, not the long one's

    REQUIRE(system.Active().size() == 1);
    REQUIRE(system.Active()[0].text == "long");
}
