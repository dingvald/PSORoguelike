#include "Engine/Render/TileVertexMath.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PixelToNdc maps the four corners of the window to NDC's corners", "[TileVertexMath]")
{
    psr::NdcPosition top_left = psr::PixelToNdc(0.0f, 0.0f, 800, 600);
    REQUIRE(top_left.x == -1.0f);
    REQUIRE(top_left.y == 1.0f);

    psr::NdcPosition bottom_right = psr::PixelToNdc(800.0f, 600.0f, 800, 600);
    REQUIRE(bottom_right.x == 1.0f);
    REQUIRE(bottom_right.y == -1.0f);
}

TEST_CASE("PixelToNdc maps the window center to the NDC origin", "[TileVertexMath]")
{
    psr::NdcPosition center = psr::PixelToNdc(400.0f, 300.0f, 800, 600);
    REQUIRE(center.x == 0.0f);
    REQUIRE(center.y == 0.0f);
}

TEST_CASE("PixelToNdc flips the Y axis (pixel space is +Y down, NDC is +Y up)", "[TileVertexMath]")
{
    psr::NdcPosition near_top = psr::PixelToNdc(0.0f, 150.0f, 800, 600);
    REQUIRE(near_top.y == 0.5f);

    psr::NdcPosition near_bottom = psr::PixelToNdc(0.0f, 450.0f, 800, 600);
    REQUIRE(near_bottom.y == -0.5f);
}

TEST_CASE("TileToPixel places camera_position's own tile at the window centre", "[TileVertexMath]")
{
    psr::PixelPosition pixel =
        psr::TileToPixel(psr::Vec2{5, 5}, psr::Vec2f{}, psr::Vec2{5, 5}, 800, 600, 16.0f, 24.0f);
    REQUIRE(pixel.x == 400.0f);
    REQUIRE(pixel.y == 300.0f);
}

TEST_CASE("TileToPixel steps by one zoomed tile per tile of distance from the camera", "[TileVertexMath]")
{
    psr::PixelPosition pixel =
        psr::TileToPixel(psr::Vec2{7, 3}, psr::Vec2f{}, psr::Vec2{5, 5}, 800, 600, 16.0f, 24.0f);
    REQUIRE(pixel.x == 400.0f + 2.0f * 16.0f);
    REQUIRE(pixel.y == 300.0f - 2.0f * 24.0f);
}

TEST_CASE("TileToPixel adds the sub-tile offset scaled by the zoomed tile size", "[TileVertexMath]")
{
    psr::PixelPosition pixel = psr::TileToPixel(psr::Vec2{5, 5}, psr::Vec2f{0.5f, -0.25f}, psr::Vec2{5, 5}, 800, 600,
                                                 16.0f, 24.0f);
    REQUIRE(pixel.x == 400.0f + 0.5f * 16.0f);
    REQUIRE(pixel.y == 300.0f - 0.25f * 24.0f);
}

TEST_CASE("TileToPixel scales tile distance by the caller's already-zoomed tile size", "[TileVertexMath]")
{
    // zoomed_tile_width/height already bake in zoom (2x here), so the same
    // one-tile distance produces twice the pixel step of the unzoomed case above.
    psr::PixelPosition pixel =
        psr::TileToPixel(psr::Vec2{6, 5}, psr::Vec2f{}, psr::Vec2{5, 5}, 800, 600, 32.0f, 48.0f);
    REQUIRE(pixel.x == 400.0f + 32.0f);
    REQUIRE(pixel.y == 300.0f);
}
