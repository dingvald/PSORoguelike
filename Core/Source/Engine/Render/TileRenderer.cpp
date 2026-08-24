#include "Engine/Render/TileRenderer.h"

#include "Engine/Math/Rect.h"
#include "Engine/Render/TileVertexMath.h"
#include "Engine/Render/Viewport.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace psr {

namespace {
    struct DrawItem
    {
        Vec2 position;
        Vec2f offset; // sub-tile render offset, tile-fraction units (see IRenderableLookup::GetRenderOffset)
        std::uint32_t texture_id = 0;
        Vec2 texture_size;
        Vec2 uv;
        Color color_1;
        Color color_2;
        int render_layer = 0;
    };
} // namespace

TileRenderer::TileRenderer(Grid& grid, TextureAtlas& atlas, TileGpuPipeline& gpu_pipeline,
                           const IRenderableLookup& renderable_lookup, int tile_width, int tile_height)
    : m_grid(&grid), m_atlas(&atlas), m_gpu_pipeline(&gpu_pipeline), m_renderable_lookup(&renderable_lookup),
      m_tile_width(tile_width), m_tile_height(tile_height)
{
}

void TileRenderer::Draw(SDL_Renderer& renderer, Vec2 camera_position, int window_width, int window_height,
                        float zoom) const
{
    if (!m_atlas->IsLoaded() || !m_gpu_pipeline->IsLoaded())
        return;

    const float zoomed_tile_width = static_cast<float>(m_tile_width) * zoom;
    const float zoomed_tile_height = static_cast<float>(m_tile_height) * zoom;

    // ComputeTileViewport's tile counts are only used below to decide which
    // cells to iterate (culling) -- never to place anything on screen.
    // Placement instead comes straight from camera_position/window centre
    // (see dst below, via TileToPixel), so it stays a continuous function of
    // zoom; if it were derived from this integer tile count instead, the
    // count's truncation would round differently at each zoom level and the
    // whole grid would visibly hop sideways as you zoomed (jitter), even
    // though the camera itself never moved. The 1-tile pad compensates for
    // that same truncation here, so culling never clips a tile placement
    // that's about to draw on-screen.
    TileViewport viewport =
        ComputeTileViewport(camera_position, window_width, window_height, static_cast<int>(zoomed_tile_width),
                            static_cast<int>(zoomed_tile_height));
    viewport.min_tile_x -= 1;
    viewport.min_tile_y -= 1;
    viewport.max_tile_x += 1;
    viewport.max_tile_y += 1;

    Rect viewport_rect{{viewport.min_tile_x, viewport.min_tile_y},
                       {viewport.max_tile_x - viewport.min_tile_x, viewport.max_tile_y - viewport.min_tile_y}};
    Rect visible = viewport_rect.Intersect(m_grid->Bounds());
    if (visible.Empty())
        return;

    std::vector<DrawItem> items;
    for (int y = visible.Top(); y < visible.Bottom(); ++y)
    {
        for (int x = visible.Left(); x < visible.Right(); ++x)
        {
            Vec2 position{x, y};
            for (entt::entity entity : m_grid->GetEntities(position))
            {
                std::optional<RenderableTile> renderable = m_renderable_lookup->GetRenderableTile(entity);
                if (!renderable)
                    continue;

                items.push_back(DrawItem{position, m_renderable_lookup->GetRenderOffset(entity),
                                         renderable->texture_id, renderable->texture_size, renderable->uv,
                                         renderable->color_1, renderable->color_2, renderable->render_layer});
            }
        }
    }

    // stable_sort, not sort: Grid::GetEntities yields a tile's occupants in
    // stamp/insertion order, and the piece editor's drag-reorder already
    // treats that order as an authored, meaningful signal for a cell's
    // stamped prefabs -- so entities sharing a render_layer on the same tile
    // should keep that order rather than being left to an unstable sort's
    // implementation-defined tie-breaking.
    std::stable_sort(items.begin(), items.end(),
                     [](const DrawItem& lhs, const DrawItem& rhs) { return lhs.render_layer < rhs.render_layer; });

    Vec2 atlas_size = m_atlas->GetSize();
    if (atlas_size.x <= 0 || atlas_size.y <= 0)
        return;

    std::vector<TileVertex> vertices;
    vertices.reserve(items.size() * 6);

    for (const DrawItem& item : items)
    {
        std::optional<SDL_FRect> src =
            m_atlas->GetSourceRect(item.texture_id, item.texture_size.x, item.texture_size.y, item.uv.x, item.uv.y);
        if (!src)
            continue;

        PixelPosition dst = TileToPixel(item.position, item.offset, camera_position, window_width, window_height,
                                        zoomed_tile_width, zoomed_tile_height);
        float dst_w = static_cast<float>(item.texture_size.x) * zoom;
        float dst_h = static_cast<float>(item.texture_size.y) * zoom;

        NdcPosition top_left = PixelToNdc(dst.x, dst.y, window_width, window_height);
        NdcPosition top_right = PixelToNdc(dst.x + dst_w, dst.y, window_width, window_height);
        NdcPosition bottom_left = PixelToNdc(dst.x, dst.y + dst_h, window_width, window_height);
        NdcPosition bottom_right = PixelToNdc(dst.x + dst_w, dst.y + dst_h, window_width, window_height);

        float u0 = src->x / static_cast<float>(atlas_size.x);
        float v0 = src->y / static_cast<float>(atlas_size.y);
        float u1 = (src->x + src->w) / static_cast<float>(atlas_size.x);
        float v1 = (src->y + src->h) / static_cast<float>(atlas_size.y);

        TileVertex top_left_vertex{top_left.x, top_left.y, u0, v0, item.color_1, item.color_2};
        TileVertex top_right_vertex{top_right.x, top_right.y, u1, v0, item.color_1, item.color_2};
        TileVertex bottom_left_vertex{bottom_left.x, bottom_left.y, u0, v1, item.color_1, item.color_2};
        TileVertex bottom_right_vertex{bottom_right.x, bottom_right.y, u1, v1, item.color_1, item.color_2};

        // cull_mode is NONE (see TileGpuPipeline), so triangle winding
        // doesn't matter here.
        vertices.push_back(top_left_vertex);
        vertices.push_back(bottom_left_vertex);
        vertices.push_back(top_right_vertex);
        vertices.push_back(top_right_vertex);
        vertices.push_back(bottom_left_vertex);
        vertices.push_back(bottom_right_vertex);
    }

    if (!vertices.empty())
        m_gpu_pipeline->Draw(renderer, *m_atlas->GetGpuTexture(), vertices, window_width, window_height);
}

} // namespace psr
