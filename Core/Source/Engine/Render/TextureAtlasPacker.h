#pragma once

#include <cstdint>
#include <vector>

namespace psr {

struct PackerInputRect
{
    std::uint32_t id = 0;
    int width = 0;
    int height = 0;
};

struct PackerPlacement
{
    std::uint32_t id = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct PackedAtlasLayout
{
    int atlas_width = 0;
    int atlas_height = 0;
    std::vector<PackerPlacement> placements;
};

// Shelf/row packer: sorts rects by height descending (stable -- ties keep
// their relative input order) then fills left-to-right shelves, wrapping to
// a new shelf when a rect doesn't fit in the remaining width. Grows the
// atlas width to fit the widest single rect if max_width is smaller than
// it, rather than clamping/cropping content.
PackedAtlasLayout PackTextureAtlas(std::vector<PackerInputRect> rects, int max_width);

} // namespace psr
