#pragma once

namespace psr {

// Fixed row-major grid layout of a texture atlas image: columns * rows cells,
// each cell_width x cell_height pixels.
struct TextureAtlasLayout
{
    int columns = 1;
    int rows = 1;
    int cell_width = 1;
    int cell_height = 1;
};

struct TextureAtlasCellRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    friend bool operator==(const TextureAtlasCellRect&, const TextureAtlasCellRect&) = default;
};

// tile_id -> (tile_id % columns, (tile_id / columns) % rows) -> pixel rect.
// Wraps both axes defensively so any non-negative tile_id maps back into a
// valid cell rather than reading outside the atlas image if a renderable
// ever asks for an id the loaded atlas doesn't cover.
//
// Inline (not a .cpp): this header must stay linkable into a Core-Test
// translation unit without dragging in TextureAtlas.cpp's SDL3_image-touching
// object file (Core-Test does not link SDL3/SDL3_image).
inline TextureAtlasCellRect ComputeAtlasCellRect(const TextureAtlasLayout& layout, int tile_id)
{
    int columns = layout.columns > 0 ? layout.columns : 1;
    int rows = layout.rows > 0 ? layout.rows : 1;
    int wrapped_id = tile_id % (columns * rows);
    if (wrapped_id < 0)
        wrapped_id += columns * rows;

    int column = wrapped_id % columns;
    int row = wrapped_id / columns;
    return TextureAtlasCellRect{column * layout.cell_width, row * layout.cell_height, layout.cell_width,
                                layout.cell_height};
}

// placement_rect: a whole source image's rect within the packed atlas.
// texture_width/texture_height: one sprite cell's size within that image.
// uv_x/uv_y: zero-based grid column/row within that image.
// No wrap/modulo here (unlike ComputeAtlasCellRect) -- there's no single
// cell count once every source image can differ in size; out-of-range uv
// is a caller error that simply produces a rect outside placement_rect.
inline TextureAtlasCellRect ComputeSpriteSourceRect(const TextureAtlasCellRect& placement_rect, int texture_width,
                                                    int texture_height, int uv_x, int uv_y)
{
    return TextureAtlasCellRect{placement_rect.x + uv_x * texture_width, placement_rect.y + uv_y * texture_height,
                                texture_width, texture_height};
}

} // namespace psr
