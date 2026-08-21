#pragma once

#include "Engine/Math/Vec2.h"
#include "Engine/Math/Vec2f.h"

namespace psr {

struct NdcPosition
{
    float x = 0.0f;
    float y = 0.0f;
};

// Converts a pixel-space coordinate (top-left origin, +Y down -- the
// convention TileRenderer's screen-space tile placement already uses) into
// SDL_GPU's NDC space (lower-left origin (-1,-1), upper-right (1,1), +Y up
// -- see SDL_gpu.h's "Coordinate System" doc block for the source
// convention; SDL_GPU normalizes backend NDC differences, so this one
// formula applies regardless of the active backend).
inline NdcPosition PixelToNdc(float pixel_x, float pixel_y, int window_width, int window_height)
{
    float ndc_x = (pixel_x / static_cast<float>(window_width)) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (pixel_y / static_cast<float>(window_height)) * 2.0f;
    return NdcPosition{ndc_x, ndc_y};
}

struct PixelPosition
{
    float x = 0.0f;
    float y = 0.0f;
};

// Places a tile position + sub-tile offset on screen, pixel space (top-left origin,
// +Y down). camera_position's own tile is pinned to the window centre; every other
// tile is placed relative to that in whole zoomed-tile steps -- continuous in zoom
// (no integer tile-count rounding), so nothing shifts on screen as zoom changes, only
// the step size does (see TileRenderer::Draw, the original site of this formula, for
// why that matters). zoomed_tile_width/height are already tile_size * zoom -- callers
// compute that once, not per placement.
inline PixelPosition TileToPixel(Vec2 position, Vec2f offset, Vec2 camera_position, int window_width, int window_height,
                                 float zoomed_tile_width, float zoomed_tile_height)
{
    float x = static_cast<float>(window_width) / 2.0f +
              (static_cast<float>(position.x - camera_position.x) + offset.x) * zoomed_tile_width;
    float y = static_cast<float>(window_height) / 2.0f +
              (static_cast<float>(position.y - camera_position.y) + offset.y) * zoomed_tile_height;
    return PixelPosition{x, y};
}

} // namespace psr
