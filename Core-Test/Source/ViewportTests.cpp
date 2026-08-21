#include "Engine/Render/Viewport.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ComputeTileViewport covers an exact 80x30 tile range for the concrete window/tile size", "[Viewport]")
{
    psr::TileViewport viewport = psr::ComputeTileViewport(psr::Vec2{0, 0}, 1280, 720, 16, 24);

    REQUIRE(viewport.min_tile_x == -40);
    REQUIRE(viewport.min_tile_y == -15);
    REQUIRE(viewport.max_tile_x == 40);
    REQUIRE(viewport.max_tile_y == 15);
}

TEST_CASE("ComputeTileViewport ceiling-rounds a non-exact-multiple window size to still cover it fully",
          "[Viewport]")
{
    psr::TileViewport viewport = psr::ComputeTileViewport(psr::Vec2{0, 0}, 1281, 721, 16, 24);

    int covered_width = (viewport.max_tile_x - viewport.min_tile_x) * 16;
    int covered_height = (viewport.max_tile_y - viewport.min_tile_y) * 24;

    REQUIRE(covered_width >= 1281);
    REQUIRE(covered_height >= 721);
}

TEST_CASE("ComputeTileViewport handles a very-negative camera position without sign bugs", "[Viewport]")
{
    psr::TileViewport viewport = psr::ComputeTileViewport(psr::Vec2{-1000, -2000}, 1280, 720, 16, 24);

    REQUIRE(viewport.min_tile_x == -1040);
    REQUIRE(viewport.min_tile_y == -2015);
    REQUIRE(viewport.max_tile_x == -960);
    REQUIRE(viewport.max_tile_y == -1985);
}
