#include "Engine/Render/Camera.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Camera defaults to the given initial position", "[Camera]")
{
    psr::Camera camera{psr::Vec2{1, 2}};

    REQUIRE(camera.GetPosition() == psr::Vec2{1, 2});
    REQUIRE_FALSE(camera.HasTarget());
}

TEST_CASE("Camera::Move offsets the free position", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    camera.Move(3, -2);

    REQUIRE(camera.GetPosition() == psr::Vec2{3, -2});
}

TEST_CASE("Camera::SetPosition replaces the free position", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    camera.SetPosition(psr::Vec2{10, 20});

    REQUIRE(camera.GetPosition() == psr::Vec2{10, 20});
}

TEST_CASE("Camera::SetTarget adopts the target position and enters tracking mode", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    camera.SetTarget(psr::Vec2{5, 5});

    REQUIRE(camera.HasTarget());
    REQUIRE(camera.GetPosition() == psr::Vec2{5, 5});
}

TEST_CASE("Camera::Move and SetPosition are ignored while tracking a target", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};
    camera.SetTarget(psr::Vec2{5, 5});

    camera.Move(1, 1);
    camera.SetPosition(psr::Vec2{99, 99});

    REQUIRE(camera.GetPosition() == psr::Vec2{5, 5});
}

TEST_CASE("Repeated SetTarget calls keep adopting the latest position", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    camera.SetTarget(psr::Vec2{5, 5});
    camera.SetTarget(psr::Vec2{6, 5});

    REQUIRE(camera.GetPosition() == psr::Vec2{6, 5});
}

TEST_CASE("Camera::ClearTarget stops tracking and keeps the last tracked position as the new free position", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};
    camera.SetTarget(psr::Vec2{5, 5});

    camera.ClearTarget();

    REQUIRE_FALSE(camera.HasTarget());
    REQUIRE(camera.GetPosition() == psr::Vec2{5, 5});

    camera.Move(1, 0);
    REQUIRE(camera.GetPosition() == psr::Vec2{6, 5});
}

TEST_CASE("Camera defaults to 1x zoom", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    REQUIRE(camera.GetZoom() == 1.0f);
}

TEST_CASE("Camera::SetZoom stores an in-range value", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    camera.SetZoom(2.0f);

    REQUIRE(camera.GetZoom() == 2.0f);
}

TEST_CASE("Camera::SetZoom clamps to [kMinCameraZoom, kMaxCameraZoom]", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    camera.SetZoom(0.1f);
    REQUIRE(camera.GetZoom() == psr::kMinCameraZoom);

    camera.SetZoom(10.0f);
    REQUIRE(camera.GetZoom() == psr::kMaxCameraZoom);
}
