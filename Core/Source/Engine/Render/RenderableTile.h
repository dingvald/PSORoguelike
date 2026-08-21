#pragma once

#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <cstdint>

namespace psr {

// What to draw for one tile-sized sprite: an atlas cell (texture_id + uv +
// texture_size) recoloured by a per-vertex two-tone palette (color_1 for
// greyscale texels < 0.5, color_2 for >= 0.5; see TileGpuPipeline). Core's
// mirror of the App's RenderableComponent, kept App-independent so Core
// mechanisms (TileRenderer, IRenderableLookup) can use it without depending
// on game content.
struct RenderableTile
{
    std::uint32_t texture_id = 0;
    Vec2 texture_size;
    Vec2 uv;
    Color color_1;
    Color color_2;
    int render_layer = 0;
};

} // namespace psr
