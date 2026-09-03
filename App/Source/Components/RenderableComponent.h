#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <cstdint>

namespace psr {

// What sprite to draw for this entity -- texture_id is the hashed_string of
// a source image's filename stem (e.g. "player" for "player.png"),
// texture_size is one cell's size within that image, and uv is the
// zero-based grid column/row within that image to draw. Source textures are
// authored greyscale (R=G=B by convention); the renderer recolors each
// texel to color_1 (grey < 0.5) or color_2 (grey >= 0.5). render_layer
// controls paint order relative to other renderables sharing a tile (e.g.
// floor, then item, then actor).
//
// frames/frame_time optionally animate uv: when frames > 1, the renderer
// cycles uv's column through a horizontal strip of frames cells (looping),
// spending frame_time seconds on each. Entities sharing the same frame_time
// change frame in lock-step (see AnimationClock). Defaults (frames = 1) mean
// "not animated" -- uv is drawn as-is.
struct RenderableComponent
{
    std::uint32_t texture_id = 0;
    Vec2 texture_size;
    Vec2 uv;
    Color color_1;
    Color color_2;
    int render_layer = 0;
    int frames = 1;
    float frame_time = 0.0f;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<RenderableComponent>("renderable")
            .Data<&RenderableComponent::texture_id>("texture_id")
            .Data<&RenderableComponent::texture_size>("texture_size")
            .Data<&RenderableComponent::uv>("uv")
            .Data<&RenderableComponent::color_1>("color_1")
            .Data<&RenderableComponent::color_2>("color_2")
            .Data<&RenderableComponent::render_layer>("render_layer")
            .Data<&RenderableComponent::frames>("frames")
            .Data<&RenderableComponent::frame_time>("frame_time");
    }
};

} // namespace psr
