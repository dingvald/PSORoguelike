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

TEST_CASE("TileToPixel defaults camera_offset to {0,0} (no smoothing applied)", "[TileVertexMath]")
{
    psr::PixelPosition with_default =
        psr::TileToPixel(psr::Vec2{5, 5}, psr::Vec2f{}, psr::Vec2{5, 5}, 800, 600, 16.0f, 24.0f);
    psr::PixelPosition with_explicit_zero =
        psr::TileToPixel(psr::Vec2{5, 5}, psr::Vec2f{}, psr::Vec2{5, 5}, 800, 600, 16.0f, 24.0f, psr::Vec2f{});
    REQUIRE(with_default.x == with_explicit_zero.x);
    REQUIRE(with_default.y == with_explicit_zero.y);
}

TEST_CASE("TileToPixel subtracts camera_offset scaled by the zoomed tile size", "[TileVertexMath]")
{
    // A camera_offset opposite in sign to a per-entity offset (same tile,
    // same magnitude) should land at the same pixel position as neither
    // being set -- the camera lagging behind by X is visually equivalent to
    // every tile shifting forward by X.
    psr::PixelPosition pixel = psr::TileToPixel(psr::Vec2{5, 5}, psr::Vec2f{}, psr::Vec2{5, 5}, 800, 600, 16.0f, 24.0f,
                                                psr::Vec2f{0.5f, -0.25f});
    REQUIRE(pixel.x == 400.0f - 0.5f * 16.0f);
    REQUIRE(pixel.y == 300.0f + 0.25f * 24.0f);
}

// All three cases below place the pixel at the tile's centre (a {0.5,0.5}
// sub-tile offset) rather than its origin corner -- PixelToTile floors to
// the containing tile, and a point safely inside a tile is robust to the
// hairline float error a multiply-then-divide round trip can introduce,
// unlike a point sitting exactly on a tile boundary.

TEST_CASE("PixelToTile inverts TileToPixel at zoom 1", "[TileVertexMath]")
{
    const psr::Vec2 tile{7, 3};
    const psr::Vec2 camera{5, 5};
    const psr::PixelPosition pixel = psr::TileToPixel(tile, psr::Vec2f{0.5f, 0.5f}, camera, 800, 600, 16.0f, 24.0f);

    const psr::Vec2 recovered = psr::PixelToTile(pixel.x, pixel.y, camera, 800, 600, 16.0f, 24.0f);
    REQUIRE(recovered == tile);
}

TEST_CASE("PixelToTile inverts TileToPixel at a non-1 zoom", "[TileVertexMath]")
{
    const psr::Vec2 tile{2, 9};
    const psr::Vec2 camera{5, 5};
    const psr::PixelPosition pixel = psr::TileToPixel(tile, psr::Vec2f{0.5f, 0.5f}, camera, 800, 600, 32.0f, 48.0f);

    const psr::Vec2 recovered = psr::PixelToTile(pixel.x, pixel.y, camera, 800, 600, 32.0f, 48.0f);
    REQUIRE(recovered == tile);
}

TEST_CASE("PixelToTile inverts TileToPixel with an off-center camera and a live camera_offset", "[TileVertexMath]")
{
    const psr::Vec2 tile{12, -4};
    const psr::Vec2 camera{10, 1};
    const psr::Vec2f camera_offset{0.3f, -0.7f};
    const psr::PixelPosition pixel =
        psr::TileToPixel(tile, psr::Vec2f{0.5f, 0.5f}, camera, 800, 600, 16.0f, 24.0f, camera_offset);

    const psr::Vec2 recovered = psr::PixelToTile(pixel.x, pixel.y, camera, 800, 600, 16.0f, 24.0f, camera_offset);
    REQUIRE(recovered == tile);
}
