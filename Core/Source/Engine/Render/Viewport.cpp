#include "Engine/Render/Viewport.h"

namespace psr {

TileViewport ComputeTileViewport(Vec2 camera_position, int window_width, int window_height, int tile_width,
                                 int tile_height)
{
    int tiles_wide = (window_width + tile_width - 1) / tile_width;
    int tiles_high = (window_height + tile_height - 1) / tile_height;

    int min_tile_x = camera_position.x - tiles_wide / 2;
    int min_tile_y = camera_position.y - tiles_high / 2;

    return TileViewport{min_tile_x, min_tile_y, min_tile_x + tiles_wide, min_tile_y + tiles_high};
}

} // namespace psr
