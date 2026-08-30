#include "Engine/Render/Camera.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

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

TEST_CASE("Camera::GetRenderOffset starts at {0,0}", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    REQUIRE(camera.GetRenderOffset() == psr::Vec2f{});
}

TEST_CASE("A reposition banks a render lag that Update() decays back toward {0,0}", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};

    camera.SetTarget(psr::Vec2{1, 0});

    // GetPosition() jumps immediately -- only the render lag trails behind.
    REQUIRE(camera.GetPosition() == psr::Vec2{1, 0});
    const psr::Vec2f lag_before_update = camera.GetRenderOffset();
    REQUIRE(lag_before_update.x < 0.0f); // camera moved +1, so it starts behind (negative lag)
    REQUIRE(lag_before_update.y == 0.0f);

    camera.Update(0.016f);

    const psr::Vec2f lag_after_update = camera.GetRenderOffset();
    REQUIRE(lag_after_update.x > lag_before_update.x); // eased toward 0
    REQUIRE(lag_after_update.x < 0.0f);                // but not fully there yet
}

TEST_CASE("Repeated Update() calls eventually settle the render lag to ~{0,0}", "[Camera]")
{
    psr::Camera camera{psr::Vec2{0, 0}};
    camera.SetTarget(psr::Vec2{5, -3});

    for (int i = 0; i < 500; ++i)
        camera.Update(0.016f);

    const psr::Vec2f lag = camera.GetRenderOffset();
    REQUIRE(std::abs(lag.x) < 0.001f);
    REQUIRE(std::abs(lag.y) < 0.001f);
}
