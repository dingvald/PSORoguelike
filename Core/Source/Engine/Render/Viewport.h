#pragma once

#include "Engine/Math/Vec2.h"

namespace psr {

// Tile-space rectangle currently visible on screen. max_tile_x/max_tile_y
// are exclusive (i.e. the rectangle is [min, max)).
struct TileViewport
{
    int min_tile_x = 0;
    int min_tile_y = 0;
    int max_tile_x = 0;
    int max_tile_y = 0;
};

// Computes the tile-space rectangle visible on screen: window_width x
// window_height pixels of window, tile_width x tile_height pixels per tile,
// centered on camera_position. Tile counts are ceiling-rounded so a window
// size that isn't an exact multiple of the tile size still gets full
// edge-to-edge coverage.
TileViewport ComputeTileViewport(Vec2 camera_position, int window_width, int window_height, int tile_width,
                                 int tile_height);

} // namespace psr
