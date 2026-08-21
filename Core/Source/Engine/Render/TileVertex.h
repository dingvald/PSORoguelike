#pragma once

#include "Engine/Math/Color.h"

namespace psr {

// One vertex of a sprite quad's triangle list. position (x, y) is already
// in clip space (NDC, [-1,1]) -- TileRenderer::Draw bakes the tile-grid ->
// screen -> NDC transform on the CPU, so TileSprite.vert.glsl is a pure
// passthrough with no uniform buffer needed.
struct TileVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    Color color_1;
    Color color_2;
};

} // namespace psr
