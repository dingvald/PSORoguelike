#pragma once

#include "Engine/Math/Vec2.h"
#include "Engine/Math/Vec2f.h"

#include <cmath>

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
// compute that once, not per placement. camera_offset is Camera::GetRenderOffset()'s
// sub-tile follow lag -- same units and sign convention as offset, but subtracted
// instead of added, since it shifts where the *camera* is looking rather than where
// the tile itself sits; defaults to {} for callers (tests, editor tooling) with no
// smoothing to apply.
inline PixelPosition TileToPixel(Vec2 position, Vec2f offset, Vec2 camera_position, int window_width, int window_height,
                                 float zoomed_tile_width, float zoomed_tile_height, Vec2f camera_offset = {})
{
    float x = static_cast<float>(window_width) / 2.0f +
              (static_cast<float>(position.x - camera_position.x) + offset.x - camera_offset.x) * zoomed_tile_width;
    float y = static_cast<float>(window_height) / 2.0f +
              (static_cast<float>(position.y - camera_position.y) + offset.y - camera_offset.y) * zoomed_tile_height;
    return PixelPosition{x, y};
}

// The algebraic inverse of TileToPixel with offset={0,0}: which integer tile
// a pixel-space point (top-left origin, +Y down) falls within, given the
// same camera_position/window/zoomed-tile-size/camera_offset a caller used
// to place tiles on screen. camera_offset should be the same live
// Camera::GetRenderOffset() TileToPixel itself is fed, so a click during
// camera ease-lag maps to the tile the player actually sees under the
// cursor, not the un-eased logical one.
inline Vec2 PixelToTile(float pixel_x, float pixel_y, Vec2 camera_position, int window_width, int window_height,
                        float zoomed_tile_width, float zoomed_tile_height, Vec2f camera_offset = {})
{
    const float local_x = (pixel_x - static_cast<float>(window_width) / 2.0f) / zoomed_tile_width +
                          static_cast<float>(camera_position.x) + camera_offset.x;
    const float local_y = (pixel_y - static_cast<float>(window_height) / 2.0f) / zoomed_tile_height +
                          static_cast<float>(camera_position.y) + camera_offset.y;
    return Vec2{static_cast<int>(std::floor(local_x)), static_cast<int>(std::floor(local_y))};
}

} // namespace psr
