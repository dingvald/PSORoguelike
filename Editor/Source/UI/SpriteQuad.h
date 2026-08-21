#pragma once

#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"
#include "Engine/Render/TileVertex.h"
#include "Engine/Render/TileVertexMath.h"

#include <SDL3/SDL.h>

#include <vector>

namespace psr {

// Appends one sprite quad (two triangles, six verts) covering dst on screen,
// sampling src (pixel-space) out of an atlas_size-sized texture. Shared by
// every content editor layer that manually draws a stack of sprites per grid
// cell (palette icons, painted-cell entities) via TileGpuPipeline::Draw
// directly rather than through Grid/TileRenderer -- which assumes exactly one
// entity per cell, not a stamped stack (the pattern UnnamedRoguelike's editor
// layers each keep their own private copy of).
inline void AppendSpriteQuad(std::vector<TileVertex>& out, const SDL_FRect& dst, const SDL_FRect& src, Vec2 atlas_size,
                             Color color_1, Color color_2, int window_width, int window_height)
{
    const NdcPosition top_left = PixelToNdc(dst.x, dst.y, window_width, window_height);
    const NdcPosition top_right = PixelToNdc(dst.x + dst.w, dst.y, window_width, window_height);
    const NdcPosition bottom_left = PixelToNdc(dst.x, dst.y + dst.h, window_width, window_height);
    const NdcPosition bottom_right = PixelToNdc(dst.x + dst.w, dst.y + dst.h, window_width, window_height);

    const float u0 = src.x / static_cast<float>(atlas_size.x);
    const float v0 = src.y / static_cast<float>(atlas_size.y);
    const float u1 = (src.x + src.w) / static_cast<float>(atlas_size.x);
    const float v1 = (src.y + src.h) / static_cast<float>(atlas_size.y);

    out.push_back(TileVertex{top_left.x, top_left.y, u0, v0, color_1, color_2});
    out.push_back(TileVertex{bottom_left.x, bottom_left.y, u0, v1, color_1, color_2});
    out.push_back(TileVertex{top_right.x, top_right.y, u1, v0, color_1, color_2});
    out.push_back(TileVertex{top_right.x, top_right.y, u1, v0, color_1, color_2});
    out.push_back(TileVertex{bottom_left.x, bottom_left.y, u0, v1, color_1, color_2});
    out.push_back(TileVertex{bottom_right.x, bottom_right.y, u1, v1, color_1, color_2});
}

// Centers a sprite_size-sized box within box, at its native (unscaled) pixel
// size -- so a small icon doesn't stretch to fill an oversized cell/slot.
inline SDL_FRect NativeSizeRect(const SDL_FRect& box, Vec2 sprite_size)
{
    const float w = static_cast<float>(sprite_size.x);
    const float h = static_cast<float>(sprite_size.y);
    return SDL_FRect{box.x + (box.w - w) * 0.5f, box.y + (box.h - h) * 0.5f, w, h};
}

} // namespace psr
