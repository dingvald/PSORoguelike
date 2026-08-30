#pragma once

#include "Engine/Render/IRenderableLookup.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "Engine/World/Grid.h"

#include <SDL3/SDL.h>

namespace psr {

// Draws the on-screen portion of grid, culled to the tile range
// ComputeTileViewport() computes from camera_position and the window size,
// intersected with the grid's own fixed bounds -- grid is a single
// non-streamed area, so every cell within that intersection is always
// "loaded".
class TileRenderer
{
public:
    // grid/atlas/gpu_pipeline/renderable_lookup must outlive this
    // TileRenderer (reference-in-ctor/pointer-member: this project's
    // established pattern for non-owning dependencies, since a raw T&
    // member would trip cppcoreguidelines-avoid-const-or-ref-data-members).
    // tile_width/tile_height: the grid's fixed screen-space step size,
    // independent of any sprite's own pixel size -- used only to position
    // tiles on screen (ComputeTileViewport, dst rect position), never as a
    // draw size, since the atlas no longer has one uniform cell size once
    // it holds differently-sized source images.
    TileRenderer(Grid& grid, TextureAtlas& atlas, TileGpuPipeline& gpu_pipeline,
                 const IRenderableLookup& renderable_lookup, int tile_width, int tile_height);

    // zoom scales both the tile grid step and each sprite's drawn size
    // around camera_position; 1.0 is the unscaled tile_width/tile_height
    // passed to the constructor. camera_offset is Camera::GetRenderOffset()'s
    // sub-tile follow lag, forwarded to TileToPixel -- see its own doc
    // comment; defaults to {} for callers with no smoothed camera.
    void Draw(SDL_Renderer& renderer, Vec2 camera_position, int window_width, int window_height, float zoom = 1.0f,
              Vec2f camera_offset = {}) const;

private:
    Grid* m_grid;
    TextureAtlas* m_atlas;
    TileGpuPipeline* m_gpu_pipeline;
    const IRenderableLookup* m_renderable_lookup;
    int m_tile_width;
    int m_tile_height;
};

} // namespace psr
